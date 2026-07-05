#include "MosaicAssembler.h"
#include "HistogramMatcher.h"
#include "SeamlineGenerator.h"
#include <gdal_priv.h>
#include <ogr_srs_api.h>
#include <cmath>
#include <algorithm>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

// ── 辅助: 地理坐标 → 像素坐标 (通用仿射逆变换) ──
static void geoToPixel(double gx, double gy, const double geo[6],
                       double& px, double& py)
{
    double det = geo[1] * geo[5] - geo[2] * geo[4];
    if (std::abs(det) < 1e-12) {
        px = -999; py = -999;
        return;
    }
    double dx = gx - geo[0];
    double dy = gy - geo[3];
    px = (dx * geo[5] - geo[2] * dy) / det;
    py = (geo[1] * dy - dx * geo[4]) / det;
}

AlgorithmResult MosaicAssembler::assemble(const MosaicParams& params,
                                           ProgressCallback progress)
{
    GDALAllRegister();
    QStringList images = params.inputImages;
    if (images.size() < 1)
        return {false, QStringLiteral("No input images for mosaic")};

    if (progress)
        progress(5, QStringLiteral("Reading image extents..."));

    const int nImages = images.size();

    // ── 1. 读取所有影像的地理信息 ──
    struct ImgInfo
    {
        QString path;
        double  geo[6] = {};
        int     w = 0, h = 0;
        double  cx = 0.0, cy = 0.0;
        double  xMin = 0.0, yMin = 0.0, xMax = 0.0, yMax = 0.0;
    };
    QVector<ImgInfo> infos(nImages);
    double env[4] = {1e30, 1e30, -1e30, -1e30};

    for (int i = 0; i < nImages; ++i)
    {
        GDALDataset* ds = (GDALDataset*)GDALOpen(
            images[i].toUtf8(), GA_ReadOnly);
        if (!ds)
            return {false, QStringLiteral("Cannot open: %1").arg(images[i])};
        infos[i].path = images[i];
        infos[i].w   = ds->GetRasterXSize();
        infos[i].h   = ds->GetRasterYSize();
        ds->GetGeoTransform(infos[i].geo);

        double x0 = infos[i].geo[0];
        double y0 = infos[i].geo[3];
        double x1 = x0 + infos[i].w * infos[i].geo[1]
                        + infos[i].h * infos[i].geo[2];
        double y1 = y0 + infos[i].w * infos[i].geo[4]
                        + infos[i].h * infos[i].geo[5];
        infos[i].xMin = std::min(x0, x1); infos[i].xMax = std::max(x0, x1);
        infos[i].yMin = std::min(y0, y1); infos[i].yMax = std::max(y0, y1);
        infos[i].cx = (infos[i].xMin + infos[i].xMax) * 0.5;
        infos[i].cy = (infos[i].yMin + infos[i].yMax) * 0.5;

        env[0] = std::min(env[0], infos[i].xMin);
        env[1] = std::min(env[1], infos[i].yMin);
        env[2] = std::max(env[2], infos[i].xMax);
        env[3] = std::max(env[3], infos[i].yMax);
        GDALClose(ds);
    }

    // ── 2. 确定输出范围和参数 ──
    double outExt[4] = {env[0], env[1], env[2], env[3]};
    if (!params.useImageExtent)
    {
        outExt[0] = params.outputExtentMinX;
        outExt[1] = params.outputExtentMinY;
        outExt[2] = params.outputExtentMaxX;
        outExt[3] = params.outputExtentMaxY;
    }
    double resX = params.outputResolutionX > 0 ? params.outputResolutionX : std::abs(infos[0].geo[1]);
    double resY = params.outputResolutionY > 0 ? params.outputResolutionY : std::abs(infos[0].geo[5]);

    // 输出 geotransform (北半球 north-up: geo[5] 为负)
    double outGeo[6] = { outExt[0], resX, 0.0, outExt[3], 0.0, -resY };

    int outW = (int)((outExt[2] - outExt[0]) / resX + 0.5);
    int outH = (int)((outExt[3] - outExt[1]) / resY + 0.5);
    if (outW <= 0 || outH <= 0)
        return {false, QStringLiteral("Invalid output dimensions: %1x%2").arg(outW).arg(outH)};

    // ── 3. 匀色 (直方图匹配) ──
    QStringList balancedPaths = images;
    if (params.colorBalanceMethod == QStringLiteral("HistogramMatching"))
    {
        QString refPath = params.histogramReferenceImage;
        if (refPath.isEmpty()) refPath = images.first();
        QString tmpDir = QFileInfo(params.outputPath).absolutePath()
                         + QStringLiteral("/balanced");
        QDir().mkpath(tmpDir);

        if (progress)
            progress(10, QStringLiteral("Color balancing..."));

        for (int i = 0; i < nImages; ++i)
        {
            if (images[i] == refPath) continue;
            QString outPath = tmpDir + QStringLiteral("/balanced_%1.tif").arg(i);
            if (HistogramMatcher::match(refPath, images[i], outPath))
                balancedPaths[i] = outPath;
        }
    }

    // ── 4. 创建输出文件 ──
    if (progress)
        progress(20, QStringLiteral("Creating output raster..."));

    int outBands = 0;
    GDALDataType outDataType = GDT_Byte;
    {
        GDALDataset* tmpDS = (GDALDataset*)GDALOpen(
            balancedPaths[0].toUtf8(), GA_ReadOnly);
        if (!tmpDS)
            return {false, QStringLiteral("Cannot open balanced image")};
        outBands = tmpDS->GetRasterCount();
        outDataType = tmpDS->GetRasterBand(1)->GetRasterDataType();
        GDALClose(tmpDS);
    }

    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!drv)
        return {false, QStringLiteral("GeoTIFF driver not available")};

    GDALDataset* outDS = drv->Create(params.outputPath.toUtf8(),
        outW, outH, outBands, outDataType, nullptr);
    if (!outDS)
        return {false, QStringLiteral("Cannot create output: %1").arg(params.outputPath)};

    outDS->SetGeoTransform(outGeo);
    {
        GDALDataset* tmpDS = (GDALDataset*)GDALOpen(
            balancedPaths[0].toUtf8(), GA_ReadOnly);
        if (tmpDS)
        {
            if (tmpDS->GetProjectionRef() && tmpDS->GetProjectionRef()[0])
                outDS->SetProjection(tmpDS->GetProjectionRef());

            // 复制源影像波段元数据 (颜色解释、无效值、比例/偏移、描述)
            for (int b = 0; b < outBands; ++b)
            {
                GDALRasterBand* srcBand = tmpDS->GetRasterBand(b + 1);
                GDALRasterBand* outBand = outDS->GetRasterBand(b + 1);

                outBand->SetColorInterpretation(
                    srcBand->GetColorInterpretation());

                int bHasNodata = 0;
                double nd = srcBand->GetNoDataValue(&bHasNodata);
                if (bHasNodata)
                    outBand->SetNoDataValue(nd);

                double scale = srcBand->GetScale(nullptr);
                double offset = srcBand->GetOffset(nullptr);
                if (scale != 1.0 || offset != 0.0)
                {
                    outBand->SetScale(scale);
                    outBand->SetOffset(offset);
                }

                const char* desc = srcBand->GetDescription();
                if (desc && desc[0])
                    outBand->SetDescription(desc);
            }
            GDALClose(tmpDS);
        }
    }

    int blockSize = params.blockSize > 0 ? params.blockSize : 512;
    float bgValue   = (float)params.backgroundValue;
    int featherPx   = (params.featheringWidth > 0)
                        ? (int)(params.featheringWidth / resX + 0.5) : 0;
    if (featherPx < 1) featherPx = 1;

    bool useFeathering = (params.featheringType != QStringLiteral("None"));
    bool useVoronoi     = (params.seamlineMethod != QStringLiteral("None"));

    // ── 5. 逐瓦片镶嵌 ──
    int tileBufMax = blockSize * blockSize * outBands;
    float* tileBuf  = new float[tileBufMax];
    int*   tileOwner = new int[blockSize * blockSize];

    int totalTiles = ((outH + blockSize - 1) / blockSize)
                   * ((outW + blockSize - 1) / blockSize);
    int tileIdx = 0;

    for (int ty = 0; ty < outH; ty += blockSize)
    {
        for (int tx = 0; tx < outW; tx += blockSize)
        {
            if (progress)
            {
                int pct = 25 + (int)(70.0 * tileIdx / std::max(1, totalTiles));
                if (!progress(pct, QStringLiteral("Tile %1/%2").arg(tileIdx + 1).arg(totalTiles)))
                {
                    delete[] tileBuf;
                    delete[] tileOwner;
                    GDALClose(outDS);
                    return {false, QStringLiteral("Cancelled by user")};
                }
            }
            tileIdx++;

            int tw = std::min(blockSize, outW - tx);
            int th = std::min(blockSize, outH - ty);
            int tpixels = tw * th;
            memset(tileBuf, 0, tpixels * outBands * sizeof(float));

            double txGeo = outGeo[0] + tx * outGeo[1];
            double tyGeo = outGeo[3] + ty * outGeo[5];

            // ── 5a. 逐像素判定归属 + 记录次近影像 (用于羽化) ──
            int*   secondBest   = new int[tpixels];
            double* bestDistArr  = new double[tpixels];
            double* secondDistArr = new double[tpixels];

            for (int p = 0; p < tpixels; ++p)
            {
                double gx = txGeo + ((p % tw) + 0.5) * resX;
                double gy = tyGeo + ((p / tw) + 0.5) * outGeo[5];

                int best = -1, sBest = -1;
                double bd = 1e30, sd = 1e30;

                if (useVoronoi)
                {
                    for (int i = 0; i < nImages; ++i)
                    {
                        if (gx < infos[i].xMin || gx > infos[i].xMax ||
                            gy < infos[i].yMin || gy > infos[i].yMax)
                            continue;
                        double dx = gx - infos[i].cx, dy = gy - infos[i].cy;
                        double d2 = dx * dx + dy * dy;
                        if (d2 < bd)
                        {
                            sd = bd; sBest = best;
                            bd = d2; best = i;
                        }
                        else if (d2 < sd)
                        {
                            sd = d2; sBest = i;
                        }
                    }
                }
                else
                {
                    // Overlay: first covering image in list order wins
                    for (int i = 0; i < nImages; ++i)
                    {
                        if (gx < infos[i].xMin || gx > infos[i].xMax ||
                            gy < infos[i].yMin || gy > infos[i].yMax)
                            continue;
                        if (best < 0)
                        {
                            best = i;
                            bd = 0.0;
                        }
                        else if (sBest < 0)
                        {
                            sBest = i;
                            double toLeft   = (gx - infos[best].xMin) / resX;
                            double toRight  = (infos[best].xMax - gx) / resX;
                            double toTop    = (infos[best].yMax - gy) / resY;
                            double toBottom = (gy - infos[best].yMin) / resY;
                            sd = std::min({toLeft, toRight, toTop, toBottom});
                            break;
                        }
                    }
                }

                tileOwner[p]     = best;
                secondBest[p]    = sBest;
                bestDistArr[p]   = bd;
                secondDistArr[p] = sd;
            }

            // ── 5b. 统计各源影像在此 tile 中的占用区域 ──
            struct SrcTileUse
            {
                bool used = false;
                int  minTX = 0, minTY = 0, maxTX = 0, maxTY = 0;
                int  srcX0 = 0, srcY0 = 0;
                int  srcW = 0, srcH = 0;
                float* buf = nullptr;
            };
            QVector<SrcTileUse> srcUse(nImages);

            for (int p = 0; p < tpixels; ++p)
            {
                int best = tileOwner[p];
                if (best < 0) continue;

                int px = p % tw, py = p / tw;
                SrcTileUse& u = srcUse[best];
                if (!u.used)
                {
                    u.used = true;
                    u.minTX = u.maxTX = px;
                    u.minTY = u.maxTY = py;
                }
                else
                {
                    u.minTX = std::min(u.minTX, px); u.maxTX = std::max(u.maxTX, px);
                    u.minTY = std::min(u.minTY, py); u.maxTY = std::max(u.maxTY, py);
                }

                // 羽化: 标记次近影像也需要读取
                if (useFeathering)
                {
                    int sBest = secondBest[p];
                    if (sBest >= 0)
                    {
                        double distToBoundary = 0.5 *
                            (std::sqrt(secondDistArr[p]) - std::sqrt(bestDistArr[p]));
                        if (distToBoundary < featherPx)
                        {
                            SrcTileUse& u2 = srcUse[sBest];
                            if (!u2.used)
                            {
                                u2.used = true;
                                u2.minTX = u2.maxTX = px;
                                u2.minTY = u2.maxTY = py;
                            }
                            else
                            {
                                u2.minTX = std::min(u2.minTX, px); u2.maxTX = std::max(u2.maxTX, px);
                                u2.minTY = std::min(u2.minTY, py); u2.maxTY = std::max(u2.maxTY, py);
                            }
                        }
                    }
                }
            }

            // ── 5c. 预读取源影像区域 ──
            GDALDataset** srcDS = new GDALDataset*[nImages];
            for (int i = 0; i < nImages; ++i)
                srcDS[i] = nullptr;

            for (int i = 0; i < nImages; ++i)
            {
                if (!srcUse[i].used) continue;

                srcDS[i] = (GDALDataset*)GDALOpen(
                    balancedPaths[i].toUtf8(), GA_ReadOnly);
                if (!srcDS[i])
                {
                    srcUse[i].used = false;
                    continue;
                }

                // 计算 tile 子区域四个角的地理坐标
                double gx0 = txGeo + (srcUse[i].minTX + 0.5) * resX;
                double gy0 = tyGeo + (srcUse[i].minTY + 0.5) * outGeo[5];
                double gx1 = txGeo + (srcUse[i].maxTX + 0.5) * resX;
                double gy1 = tyGeo + (srcUse[i].maxTY + 0.5) * outGeo[5];

                double spx0, spy0, spx1, spy1;
                geoToPixel(gx0, gy0, infos[i].geo, spx0, spy0);
                geoToPixel(gx1, gy1, infos[i].geo, spx1, spy1);

                int sx0 = (int)std::floor(std::min(spx0, spx1));
                int sy0 = (int)std::floor(std::min(spy0, spy1));
                int sx1 = (int)std::ceil(std::max(spx0, spx1));
                int sy1 = (int)std::ceil(std::max(spy0, spy1));

                // 钳制到源影像范围
                sx0 = std::max(0, std::min(infos[i].w, sx0));
                sy0 = std::max(0, std::min(infos[i].h, sy0));
                sx1 = std::max(0, std::min(infos[i].w, sx1));
                sy1 = std::max(0, std::min(infos[i].h, sy1));

                int sw = sx1 - sx0;
                int sh = sy1 - sy0;
                if (sw <= 0 || sh <= 0)
                {
                    srcUse[i].used = false;
                    continue;
                }

                srcUse[i].srcX0 = sx0;
                srcUse[i].srcY0 = sy0;
                srcUse[i].srcW  = sw;
                srcUse[i].srcH  = sh;
                srcUse[i].buf   = new float[sw * sh * outBands];

                // 逐波段读取 (BIP 布局)
                for (int b = 0; b < outBands; ++b)
                {
                    CPLErr e = srcDS[i]->GetRasterBand(b + 1)->RasterIO(
                        GF_Read, sx0, sy0, sw, sh,
                        srcUse[i].buf + b, sw, sh,
                        GDT_Float32,
                        sizeof(float) * outBands,
                        sizeof(float) * sw * outBands);
                    if (e != CE_None)
                    {
                        delete[] srcUse[i].buf;
                        srcUse[i].buf = nullptr;
                        srcUse[i].used = false;
                        break;
                    }
                }
            }

            // ── 5d. 填充 tile 缓冲区 ──
            for (int p = 0; p < tpixels; ++p)
            {
                int best = tileOwner[p];
                int px = p % tw, py = p / tw;

                // 使用像素中心坐标 (与 5a 阶段保持一致)
                double gx = txGeo + (px + 0.5) * resX;
                double gy = tyGeo + (py + 0.5) * outGeo[5];

                // 羽化混合
                if (useFeathering && best >= 0)
                {
                    int sBest = secondBest[p];
                    if (sBest >= 0 && srcUse[sBest].used && srcUse[best].used)
                    {
                        // 像素到拼接线边界的距离
                        double distToBoundary = useVoronoi
                            ? 0.5 * (std::sqrt(secondDistArr[p]) - std::sqrt(bestDistArr[p]))
                            : secondDistArr[p];

                        if (distToBoundary < featherPx)
                        {
                            // 距离边界越远, 最近影像权重越大
                            double wBest = 0.5 + 0.5 * (distToBoundary / featherPx);
                            double wSecond = 1.0 - wBest;

                            double spx1, spy1, spx2, spy2;
                            geoToPixel(gx, gy, infos[best].geo, spx1, spy1);
                            geoToPixel(gx, gy, infos[sBest].geo, spx2, spy2);

                            int sx1 = srcUse[best].srcX0, sy1 = srcUse[best].srcY0;
                            int sx2 = srcUse[sBest].srcX0, sy2 = srcUse[sBest].srcY0;

                            double bx1 = spx1 - sx1, by1 = spy1 - sy1;
                            double bx2 = spx2 - sx2, by2 = spy2 - sy2;

                            if (bx1 >= 0 && bx1 < srcUse[best].srcW &&
                                by1 >= 0 && by1 < srcUse[best].srcH &&
                                bx2 >= 0 && bx2 < srcUse[sBest].srcW &&
                                by2 >= 0 && by2 < srcUse[sBest].srcH)
                            {
                                int ix1 = (int)bx1, iy1 = (int)by1;
                                int ix2 = (int)bx2, iy2 = (int)by2;
                                int sw1 = srcUse[best].srcW;
                                int sw2 = srcUse[sBest].srcW;
                                int nB = outBands;

                                for (int b = 0; b < nB; ++b)
                                {
                                    float val1 = srcUse[best].buf[(iy1 * sw1 + ix1) * nB + b];
                                    float val2 = srcUse[sBest].buf[(iy2 * sw2 + ix2) * nB + b];
                                    tileBuf[p * nB + b] = val1 * wBest + val2 * wSecond;
                                }
                                continue;
                            }
                        }
                    }
                }

                // 单源取像 (最近邻采样)
                if (best >= 0 && srcUse[best].used)
                {
                    double spx, spy;
                    geoToPixel(gx, gy, infos[best].geo, spx, spy);
                    double bx = spx - srcUse[best].srcX0;
                    double by = spy - srcUse[best].srcY0;

                    if (bx >= 0 && bx < srcUse[best].srcW &&
                        by >= 0 && by < srcUse[best].srcH)
                    {
                        int ix = (int)bx, iy = (int)by;
                        int sw = srcUse[best].srcW;
                        int nB = outBands;
                        for (int b = 0; b < nB; ++b)
                            tileBuf[p * nB + b] = srcUse[best].buf[(iy * sw + ix) * nB + b];
                        continue;
                    }
                }

                // 背景填充
                for (int b = 0; b < outBands; ++b)
                    tileBuf[p * outBands + b] = bgValue;
            }

            // ── 5e. 写入输出 ──
            for (int b = 0; b < outBands; ++b)
            {
                CPLErr we = outDS->GetRasterBand(b + 1)->RasterIO(
                    GF_Write, tx, ty, tw, th,
                    tileBuf + b, tw, th,
                    GDT_Float32,
                    sizeof(float) * outBands,
                    sizeof(float) * tw * outBands);
                if (we != CE_None)
                {
                    // 清理所有已打开资源后返回
                    for (int i = 0; i < nImages; ++i)
                    {
                        delete[] srcUse[i].buf;
                        if (srcDS[i]) GDALClose(srcDS[i]);
                    }
                    delete[] srcDS;
                    delete[] tileOwner;
                    delete[] bestDistArr;
                    delete[] secondDistArr;
                    delete[] secondBest;
                    delete[] tileBuf;
                    GDALClose(outDS);
                    return {false, QStringLiteral("Write error at tile %1").arg(tileIdx)};
                }
            }

            // ── 清理当前 tile 资源 ──
            for (int i = 0; i < nImages; ++i)
            {
                delete[] srcUse[i].buf;
                if (srcDS[i]) GDALClose(srcDS[i]);
            }
            delete[] srcDS;
            delete[] bestDistArr;
            delete[] secondDistArr;
            delete[] secondBest;
        }
    }

    delete[] tileBuf;
    delete[] tileOwner;
    // 计算输出影像统计值, 确保GIS查看器正确拉伸显示
    // 计算并持久化输出影像统计值, 确保GIS查看器正确拉伸显示
    for (int b = 0; b < outBands; ++b)
    {
        double dfMin = 0, dfMax = 0, dfMean = 0, dfStdDev = 0;
        CPLErr e = outDS->GetRasterBand(b + 1)->ComputeStatistics(
            FALSE, &dfMin, &dfMax, &dfMean, &dfStdDev, nullptr, nullptr);
        if (e == CE_None)
        {
            outDS->GetRasterBand(b + 1)->SetStatistics(dfMin, dfMax, dfMean, dfStdDev);
            qDebug() << "[Mosaic] Band" << (b + 1) << "stats: min=" << dfMin
                     << "max=" << dfMax << "mean=" << dfMean << "stddev=" << dfStdDev;
        }
    }

    // 若源影像未设置颜色解释, 为 3+ 波段补充 RGB 默认值
    if (outBands >= 3)
    {
        for (int b = 0; b < 3; ++b)
        {
            if (outDS->GetRasterBand(b + 1)->GetColorInterpretation() == GCI_Undefined)
            {
                static const GDALColorInterp defInterp[3] = {
                    GCI_RedBand, GCI_GreenBand, GCI_BlueBand};
                outDS->GetRasterBand(b + 1)->SetColorInterpretation(defInterp[b]);
            }
        }
    }

    GDALClose(outDS);

    // ── 6. 生成镶嵌线矢量文件 ──
    if (params.seamlineMethod != QStringLiteral("None"))
    {
        if (progress)
            progress(96, QStringLiteral("Generating seamline vectors..."));

        double seamRes = (resX + resY) * 0.5;
        double seamExt[4] = {outExt[0], outExt[1], outExt[2], outExt[3]};

        SeamlineMask seamMask = SeamlineGenerator::generateVoronoi(
            images, seamExt, seamRes);

        QFileInfo fi(params.outputPath);
        QString shpPath = fi.absolutePath() + "/" + fi.completeBaseName()
                          + "_seamlines.shp";

        if (SeamlineGenerator::exportSeamlinesToShapefile(seamMask, images, shpPath))
        {
            qDebug() << "[Mosaic] Seamline shapefile written to:" << shpPath;
        }
        else
        {
            qWarning() << "[Mosaic] Failed to generate seamline shapefile";
        }

        seamMask.release();
    }

    if (progress)
        progress(100, QStringLiteral("Mosaic complete"));

    return {true, params.outputPath};
}
