#include "GeometricCorrector.h"
#include "GcpModelSolver.h"
#include "GcpMatcher.h"
#include "ResampleEngine.h"
#include "cuda/ResampleKernels.h"
#include "algorithms/common/GeoTransformUtils.h"

#include <gdal_priv.h>
#include <ogr_srs_api.h>
#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDir>
#include <cmath>

// ── 从 GDAL 数据集读取像元大小 ──
static QPair<double,double> readPixelSize(GDALDataset* ds)
{
    double gt[6];
    if (ds->GetGeoTransform(gt) == CE_None)
        return {std::abs(gt[1]), std::abs(gt[5])};
    return {0, 0};
}

// ── 影像四角地理范围 ──
static void imageGeoBounds(GDALDataset* ds, double bounds[4])
{
    double gt[6];
    ds->GetGeoTransform(gt);
    int w = ds->GetRasterXSize();
    int h = ds->GetRasterYSize();

    double x0 = gt[0];                        double y0 = gt[3];
    double x1 = gt[0] + w * gt[1];            double y1 = gt[3] + w * gt[4];
    double x2 = gt[0] + h * gt[2];            double y2 = gt[3] + h * gt[5];
    double x3 = gt[0] + w * gt[1] + h * gt[2]; double y3 = gt[3] + w * gt[4] + h * gt[5];

    bounds[0] = std::min({x0, x1, x2, x3});
    bounds[1] = std::min({y0, y1, y2, y3});
    bounds[2] = std::max({x0, x1, x2, x3});
    bounds[3] = std::max({y0, y1, y2, y3});
}

// ── Compute source image sub-region needed to produce an output tile ──
// Samples correction model on a grid across the tile, finds min/max src coords,
// adds margin for interpolation method.
static void computeSourceRegion(
    const GcpModel& correctionModel,
    const QVector<Gcp>& gcpsForTPS,     // for TPS: swapped GCPs where .srcX/.srcY = ref coords
    const double* tileGeo,
    int tw, int th,
    int resampleIdx,
    int fullSrcW, int fullSrcH,
    int& outSrcX, int& outSrcY, int& outSrcW, int& outSrcH)
{
    double srcMinX = 1e30, srcMinY = 1e30, srcMaxX = -1e30, srcMaxY = -1e30;
    const int samples = 5;

    for (int sy = 0; sy < samples; ++sy)
    {
        for (int sx = 0; sx < samples; ++sx)
        {
            double col = (double)sx / (samples - 1) * (tw - 1);
            double row = (double)sy / (samples - 1) * (th - 1);
            double geoX = tileGeo[0] + col * tileGeo[1] + row * tileGeo[2];
            double geoY = tileGeo[3] + col * tileGeo[4] + row * tileGeo[5];

            double srcX, srcY;
            GcpModelSolver::evalModel(correctionModel, gcpsForTPS, geoX, geoY, srcX, srcY);

            srcMinX = std::min(srcMinX, srcX);
            srcMinY = std::min(srcMinY, srcY);
            srcMaxX = std::max(srcMaxX, srcX);
            srcMaxY = std::max(srcMaxY, srcY);
        }
    }

    int margin = (resampleIdx == 2) ? 2 : ((resampleIdx == 1) ? 1 : 0);
    outSrcX = std::max(0, (int)std::floor(srcMinX) - margin);
    outSrcY = std::max(0, (int)std::floor(srcMinY) - margin);
    int x1 = std::min(fullSrcW - 1, (int)std::ceil(srcMaxX) + margin);
    int y1 = std::min(fullSrcH - 1, (int)std::ceil(srcMaxY) + margin);
    outSrcW = x1 - outSrcX + 1;
    outSrcH = y1 - outSrcY + 1;
    if (outSrcW < 1) outSrcW = 1;
    if (outSrcH < 1) outSrcH = 1;
}

// ── Read a sub-region of the source image (BIP layout, float) ──
static float* readSourceRegion(GDALDataset* srcDS, int bands,
                                int sx, int sy, int sw, int sh)
{
    float* buf = new float[(size_t)sw * sh * bands];
    for (int b = 0; b < bands; ++b)
    {
        CPLErr e = srcDS->GetRasterBand(b + 1)->RasterIO(
            GF_Read, sx, sy, sw, sh,
            buf + b, sw, sh,
            GDT_Float32, sizeof(float) * bands, sizeof(float) * sw * bands);
        if (e != CE_None)
        {
            delete[] buf;
            return nullptr;
        }
    }
    return buf;
}

// ═══════════════════════════════════════════════════
//  fitCorrectionModel: 拟合 ref→src 方向的校正模型
// ═══════════════════════════════════════════════════

GcpModel GeometricCorrector::fitCorrectionModel(const QVector<Gcp>& gcps,
                                                const QString& modelType)
{
    if (gcps.isEmpty())
        return {};

    // 交换 src/ref 角色：用 ref 坐标预测 src 坐标
    QVector<Gcp> swapped = gcps;
    for (auto& g : swapped)
    {
        std::swap(g.srcX, g.refX);
        std::swap(g.srcY, g.refY);
    }

    if (modelType == "TPS")
        return GcpModelSolver::fitTPS(swapped);

    int order = 2;
    // "Polynomial1" → 1
    if (modelType.startsWith("Polynomial"))
        order = modelType.mid(10).toInt();

    return GcpModelSolver::fitPolynomial(swapped, order);
}

// ═══════════════════════════════════════════════════
//  computeOutputExtent: 确定输出影像范围
// ═══════════════════════════════════════════════════

bool GeometricCorrector::computeOutputExtent(
    const QString& srcImage, const QString& refImage,
    const GcpModel& forwardModel,
    const GeometricCorrectionParams& params,
    double extent[4])
{
    // 用户已指定范围
    if (params.outputExtent[2] > params.outputExtent[0] &&
        params.outputExtent[3] > params.outputExtent[1])
    {
        extent[0] = params.outputExtent[0];
        extent[1] = params.outputExtent[1];
        extent[2] = params.outputExtent[2];
        extent[3] = params.outputExtent[3];
        return true;
    }

    // 自动计算：优先使用参考影像范围，否则使用源影像范围
    GDALAllRegister();
    QString img = refImage.isEmpty() ? srcImage : refImage;
    GDALDataset* ds = (GDALDataset*)GDALOpen(img.toUtf8().constData(), GA_ReadOnly);
    if (!ds)
    {
        qWarning() << "[GeometricCorrector] Cannot open" << img << "for extent";
        return false;
    }

    imageGeoBounds(ds, extent);
    GDALClose(ds);
    return true;
}

// ═══════════════════════════════════════════════════
//  detectGcps: 仅执行 GCP 匹配
// ═══════════════════════════════════════════════════

QVector<Gcp> GeometricCorrector::detectGcps(const GeometricCorrectionParams& params)
{
    if (params.matchingMode == "Manual" || params.matchingMode == "SemiAuto")
        return params.gcps;

    return GcpMatcher::autoMatch(
        params.sourceImage, params.referenceImage, params.matching);
}

// ═══════════════════════════════════════════════════
//  correct: 主入口
// ═══════════════════════════════════════════════════

GeometricResult GeometricCorrector::correct(
    const GeometricCorrectionParams& params,
    ProgressCallback progress)
{
    GeometricResult result;
    QElapsedTimer totalTimer;
    totalTimer.start();

    GDALAllRegister();

    // ── 1. 打开源影像 ──
    GDALDataset* srcDS = (GDALDataset*)GDALOpen(
        params.sourceImage.toUtf8().constData(), GA_ReadOnly);
    if (!srcDS)
    {
        result.errorMessage = QString("Cannot open source: %1").arg(params.sourceImage);
        return result;
    }

    int srcW = srcDS->GetRasterXSize();
    int srcH = srcDS->GetRasterYSize();
    int srcBands = srcDS->GetRasterCount();
    GDALDataType srcDT = srcDS->GetRasterBand(1)->GetRasterDataType();

    // ── 参数处理：像元大小默认值 ──
    double pxSizeX = params.outputPixelSizeX;
    double pxSizeY = params.outputPixelSizeY;
    if (pxSizeX <= 0 || pxSizeY <= 0)
    {
        auto ps = readPixelSize(srcDS);
        if (pxSizeX <= 0) pxSizeX = ps.first;
        if (pxSizeY <= 0) pxSizeY = ps.second;
        if (pxSizeX <= 0) pxSizeX = 1.0;
        if (pxSizeY <= 0) pxSizeY = 1.0;
    }

    if (progress && !progress(2, "Reading images..."))
    {
        GDALClose(srcDS);
        result.errorMessage = "Cancelled";
        return result;
    }

    // ── 2. GCP 获取 ──
    QVector<Gcp> gcps;
    int totalMatches = 0, inliers = 0;

    if (progress)
        progress(5, "Matching GCPs...");

    QElapsedTimer matchTimer;
    matchTimer.start();

    if (params.matchingMode == "Auto")
    {
        gcps = GcpMatcher::autoMatch(
            params.sourceImage, params.referenceImage,
            params.matching, &totalMatches, &inliers);
    }
    else if (params.matchingMode == "SemiAuto")
    {
        gcps = GcpMatcher::refineByNCC(
            params.sourceImage, params.referenceImage,
            params.gcps, params.matching);
        totalMatches = gcps.size();
        inliers = gcps.size();
    }
    else // Manual
    {
        gcps = params.gcps;
        totalMatches = gcps.size();
        inliers = gcps.size();
    }

    result.matchTimeSec = matchTimer.elapsed() / 1000.0;
    result.totalGcps = totalMatches;
    result.inlierGcps = inliers;

    if (progress && !progress(15, QString("GCPs: %1 inliers").arg(inliers)))
    {
        GDALClose(srcDS);
        result.errorMessage = "Cancelled";
        return result;
    }

    // 检查最少 GCP 数量
    int minGcp = GcpModelSolver::minGcpCountPolynomial(
        params.modelType.startsWith("Polynomial")
            ? params.modelType.mid(10).toInt() : 2);
    if (params.modelType == "TPS")
        minGcp = GcpModelSolver::minGcpCountTPS();

    if (gcps.size() < minGcp)
    {
        GDALClose(srcDS);
        result.errorMessage = QString("Insufficient GCPs: need %1, got %2")
            .arg(minGcp).arg(gcps.size());
        return result;
    }

    // Convert ref GCP coords from pixel to geographic (SIFT/SURF/NCC give pixel coords)
    if ((params.matchingMode == "Auto" || params.matchingMode == "SemiAuto")
        && !params.referenceImage.isEmpty())
    {
        GDALDataset* refDS = (GDALDataset*)GDALOpen(
            params.referenceImage.toUtf8().constData(), GA_ReadOnly);
        if (refDS)
        {
            double refGeo[6];
            if (refDS->GetGeoTransform(refGeo) == CE_None)
            {
                for (auto& gcp : gcps)
                {
                    double px = gcp.refX;
                    double py = gcp.refY;
                    gcp.refX = refGeo[0] + px * refGeo[1] + py * refGeo[2];
                    gcp.refY = refGeo[3] + px * refGeo[4] + py * refGeo[5];
                }
            }
            GDALClose(refDS);
        }
    }

    // ── 3. 模型拟合 ──
    if (progress)
        progress(20, "Fitting correction model...");

    // 3a. 前向模型 (src→ref)，用于 RMSE
    GcpModel forwardModel;
    if (params.modelType == "TPS")
        forwardModel = GcpModelSolver::fitTPS(gcps);
    else
        forwardModel = GcpModelSolver::fitPolynomial(gcps,
            params.modelType.mid(10).toInt());

    if (forwardModel.coefficients.isEmpty())
    {
        GDALClose(srcDS);
        result.errorMessage = "Model fitting failed";
        return result;
    }

    // 3b. 计算 RMSE
    QVector<Gcp> gcpsForRms = gcps;
    double rmse = GcpModelSolver::computeRMSE(gcpsForRms, forwardModel);
    forwardModel.overallRmse = rmse;
    result.finalGcps = gcpsForRms;
    result.model = forwardModel;

    // 3c. 校正方向模型 (ref→src)
    GcpModel correctionModel = fitCorrectionModel(gcps, params.modelType);

    if (progress && !progress(25, QString("Model RMSE: %1 px").arg(rmse, 0, 'f', 3)))
    {
        GDALClose(srcDS);
        result.errorMessage = "Cancelled";
        return result;
    }

    // ── 4. 确定输出范围 ──
    double extent[4];
    if (!computeOutputExtent(params.sourceImage, params.referenceImage,
                             forwardModel, params, extent))
    {
        GDALClose(srcDS);
        result.errorMessage = "Cannot determine output extent";
        return result;
    }

    int outW = (int)((extent[2] - extent[0]) / pxSizeX + 0.5);
    int outH = (int)((extent[3] - extent[1]) / pxSizeY + 0.5);
    double outGeo[6] = { extent[0], pxSizeX, 0.0, extent[3], 0.0, -pxSizeY };

    if (outW <= 0 || outH <= 0)
    {
        GDALClose(srcDS);
        result.errorMessage = QString("Invalid output size: %1x%2").arg(outW).arg(outH);
        return result;
    }

    // ── 5. 创建输出文件 ──
    if (progress)
        progress(28, "Creating output raster...");

    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!drv)
    {
        GDALClose(srcDS);
        result.errorMessage = "GeoTIFF driver not available";
        return result;
    }

    GDALDataset* outDS = drv->Create(params.outputPath.toUtf8(),
        outW, outH, srcBands, srcDT, nullptr);
    if (!outDS)
    {
        GDALClose(srcDS);
        result.errorMessage = QString("Cannot create output: %1").arg(params.outputPath);
        return result;
    }

    outDS->SetGeoTransform(outGeo);
    const char* proj = srcDS->GetProjectionRef();
    if (proj && proj[0])
        outDS->SetProjection(proj);

    // 查找/设置 NODATA
    int hasNDV = 0;
    double ndv = srcDS->GetRasterBand(1)->GetNoDataValue(&hasNDV);
    float ndvF = hasNDV ? (float)ndv : 0.0f;

    // ── 6. 准备逐块处理 ──
    int blockSize = params.blockSize > 0 ? params.blockSize : 512;
    int resampleIdx = 0;
    if (params.resampleMethod == "Bilinear") resampleIdx = 1;
    else if (params.resampleMethod == "Cubic") resampleIdx = 2;

    // Build swapped GCPs for TPS correction model evaluation
    // (correction model was fitted with swapped GCPs, so srcX/srcY = ref coords)
    QVector<Gcp> correctionGcps = gcps;
    for (auto& g : correctionGcps)
    {
        std::swap(g.srcX, g.refX);
        std::swap(g.srcY, g.refY);
    }

    // TPS kernel center coords for GPU (ref coords)
    QVector<double> gcpRefCoords;
    if (correctionModel.type == "TPS")
    {
        gcpRefCoords.resize(2 * gcps.size());
        for (int i = 0; i < gcps.size(); ++i)
        {
            gcpRefCoords[2 * i]     = gcps[i].refX;
            gcpRefCoords[2 * i + 1] = gcps[i].refY;
        }
    }

    QElapsedTimer correctTimer;
    correctTimer.start();

    float* tileBuf = new float[(size_t)blockSize * blockSize * srcBands];
    int totalTiles = ((outH + blockSize - 1) / blockSize)
                   * ((outW + blockSize - 1) / blockSize);
    int tileIdx = 0;

    bool gpuAvail = ResampleEngine::gpuAvailable();
    bool useDevicePath = gpuAvail;

    for (int ty = 0; ty < outH; ty += blockSize)
    {
        for (int tx = 0; tx < outW; tx += blockSize)
        {
            if (progress)
            {
                int pct = 30 + (int)(65.0 * tileIdx / std::max(1, totalTiles));
                if (!progress(pct, QString("Tile %1/%2 %3")
                    .arg(tileIdx + 1).arg(totalTiles)
                    .arg(useDevicePath ? "(GPU)" : "(CPU)")))
                {
                    delete[] tileBuf;
                    GDALClose(srcDS);
                    GDALClose(outDS);
                    result.errorMessage = "Cancelled";
                    return result;
                }
            }
            tileIdx++;

            int tw = std::min(blockSize, outW - tx);
            int th = std::min(blockSize, outH - ty);

            // Build tile geo-transform
            double tileGeo[6];
            for (int i = 0; i < 6; ++i) tileGeo[i] = outGeo[i];
            tileGeo[0] = outGeo[0] + tx * outGeo[1];
            tileGeo[3] = outGeo[3] + ty * outGeo[5];

            // Compute source region needed for this tile
            int srcRegX, srcRegY, srcRegW, srcRegH;
            computeSourceRegion(correctionModel, correctionGcps,
                tileGeo, tw, th, resampleIdx,
                srcW, srcH, srcRegX, srcRegY, srcRegW, srcRegH);

            // Read only the needed source sub-region
            float* tileSrcBuf = readSourceRegion(srcDS, srcBands,
                srcRegX, srcRegY, srcRegW, srcRegH);
            if (!tileSrcBuf)
            {
                delete[] tileBuf;
                GDALClose(srcDS);
                GDALClose(outDS);
                result.errorMessage = "Read source region failed";
                return result;
            }

            bool ok = false;

            if (useDevicePath)
            {
                // GPU path: upload tile source region, process, download result
                size_t tileSrcBytes = (size_t)srcRegW * srcRegH * srcBands * sizeof(float);
                size_t tileDstBytes = (size_t)tw * th * srcBands * sizeof(float);
                float* d_tileSrc = nullptr;
                float* d_tile = nullptr;

                if (cudaUploadSource(tileSrcBuf, tileSrcBytes, &d_tileSrc) == 0 &&
                    cudaAllocDevice(tileDstBytes, &d_tile) == 0)
                {
                    int ret = -1;
                    if (correctionModel.type != "TPS")
                    {
                        int polyOrder = correctionModel.type.mid(10).toInt();
                        ret = cudaResampleTilePoly_device(
                            d_tileSrc, srcRegW, srcRegH, srcBands,
                            srcRegX, srcRegY,
                            correctionModel.coefficients.constData(), polyOrder,
                            tileGeo, resampleIdx, ndvF,
                            d_tile, tx, ty, tw, th);
                    }
                    else
                    {
                        ret = cudaResampleTileTPS_device(
                            d_tileSrc, srcRegW, srcRegH, srcBands,
                            srcRegX, srcRegY,
                            correctionModel.coefficients.constData(),
                            gcpRefCoords.constData(), gcps.size(),
                            tileGeo, resampleIdx, ndvF,
                            d_tile, tx, ty, tw, th);
                    }
                    if (ret == 0)
                        ok = (cudaCopyFromDevice(tileBuf, d_tile, tileDstBytes) == 0);

                    if (d_tile) cudaFreeDevicePtr(d_tile);
                    if (d_tileSrc) cudaFreeDevicePtr(d_tileSrc);
                }
                else
                {
                    // GPU alloc/upload failed for this tile — disable GPU for remaining tiles
                    if (d_tileSrc) cudaFreeDevicePtr(d_tileSrc);
                    if (d_tile) cudaFreeDevicePtr(d_tile);
                    useDevicePath = false;
                    result.gpuUsed = false;
                }
            }

            if (!ok)
            {
                // CPU fallback (either GPU disabled or GPU processing failed)
                ok = ResampleEngine::processTile(
                    tileSrcBuf, srcRegW, srcRegH, srcBands,
                    srcRegX, srcRegY,
                    tileGeo, correctionModel, gcps,
                    params.resampleMethod, ndvF,
                    tileBuf, tx, ty, tw, th);
            }

            delete[] tileSrcBuf;

            if (!ok)
            {
                delete[] tileBuf;
                GDALClose(srcDS);
                GDALClose(outDS);
                result.errorMessage = "GPU/CPU processing failed";
                return result;
            }

            for (int b = 0; b < srcBands; ++b)
            {
                outDS->GetRasterBand(b + 1)->RasterIO(
                    GF_Write, tx, ty, tw, th,
                    tileBuf + b, tw, th,
                    GDT_Float32,
                    sizeof(float) * srcBands, sizeof(float) * tw * srcBands);
            }
        }
    }

    result.correctTimeSec = correctTimer.elapsed() / 1000.0;
    result.gpuUsed = useDevicePath;

    // ── 7. 计算统计值 ──
    if (progress)
        progress(97, "Computing statistics...");

    for (int b = 0; b < srcBands; ++b)
    {
        double dmin, dmax, dmean, dstd;
        outDS->GetRasterBand(b + 1)->ComputeStatistics(
            FALSE, &dmin, &dmax, &dmean, &dstd, nullptr, nullptr);
        outDS->GetRasterBand(b + 1)->SetStatistics(dmin, dmax, dmean, dstd);
    }

    // ── 清理 ──
    delete[] tileBuf;
    GDALClose(srcDS);
    GDALClose(outDS);

    result.success = true;
    result.outputPath = params.outputPath;

    if (progress)
        progress(100, QString("Complete: RMSE=%1, %2 GCPs, GPU=%3")
            .arg(rmse, 0, 'f', 3).arg(inliers).arg(gpuAvail ? "Yes" : "No"));

    qDebug() << "[GeometricCorrector] Done. RMSE:" << rmse
             << "GCPs:" << inliers << "/" << totalMatches
             << "Match:" << result.matchTimeSec << "s"
             << "Correct:" << result.correctTimeSec << "s"
             << "GPU:" << gpuAvail
             << "Size:" << outW << "x" << outH;

    return result;
}
