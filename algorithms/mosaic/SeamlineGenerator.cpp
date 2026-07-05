#include "SeamlineGenerator.h"
#include <gdal_priv.h>
#include <gdal_alg.h>
#include <ogr_srs_api.h>
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <QDebug>
#include <QFileInfo>

SeamlineMask SeamlineGenerator::generateVoronoi(const QStringList& imagePaths,
                                                  const double outputExtent[4],
                                                  double resolution)
                                                  {
    SeamlineMask mask;
    if (imagePaths.isEmpty()) return mask;

    GDALAllRegister();
    int nImages = imagePaths.size();

    // 读取每景影像的地理范围 + 中心点
    struct ImgInfo { double xMin, yMin, xMax, yMax, cx, cy; };
    QVector<ImgInfo> infos(nImages);
    double env[4] = {1e30, 1e30, -1e30, -1e30};

    for (int i = 0; i < nImages; ++i)
    {
        GDALDataset* ds = (GDALDataset*)GDALOpen(
            imagePaths[i].toUtf8(), GA_ReadOnly);
        if (!ds) continue;

        double geo[6]; ds->GetGeoTransform(geo);
        int w = ds->GetRasterXSize(), h = ds->GetRasterYSize();

        // 读取投影 (从首景成功打开的影像获取)
        if (mask.projection.isEmpty() && ds->GetProjectionRef()
            && ds->GetProjectionRef()[0])
        {
            mask.projection = QString::fromUtf8(ds->GetProjectionRef());
        }

        double x0 = geo[0], y0 = geo[3];
        double x1 = x0 + w * geo[1] + h * geo[2];
        double y1 = y0 + w * geo[4] + h * geo[5];
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

    // 输出范围
    double xMin = (outputExtent && outputExtent[2] > outputExtent[0])
        ? outputExtent[0] : env[0];
    double yMin = (outputExtent && outputExtent[3] > outputExtent[1])
        ? outputExtent[1] : env[1];
    double xMax = (outputExtent && outputExtent[2] > outputExtent[0])
        ? outputExtent[2] : env[2];
    double yMax = (outputExtent && outputExtent[3] > outputExtent[1])
        ? outputExtent[3] : env[3];
    if (resolution <= 0) resolution = 30.0;

    // 限制掩膜最大尺寸, 避免内存过大和 shapefile 顶点过多
    double extentW = xMax - xMin;
    double extentH = yMax - yMin;
    const int maxMaskPixels = 3000;
    double maxExtent = std::max(extentW, extentH);
    if (maxExtent / resolution > maxMaskPixels)
        resolution = maxExtent / maxMaskPixels;

    mask.width  = (int)((extentW / resolution) + 0.5);
    mask.height = (int)((extentH / resolution) + 0.5);
    mask.geoTransform[0] = xMin; mask.geoTransform[1] = resolution;
    mask.geoTransform[2] = 0;    mask.geoTransform[3] = yMax;
    mask.geoTransform[4] = 0;    mask.geoTransform[5] = -resolution;

    // 分配每景影像的掩膜
    int pixels = mask.width * mask.height;
    for (int i = 0; i < nImages; ++i)
    {
        float* m = new float[pixels]{};
        mask.imageMasks.append(m);
    }

    // 逐像素分配：归属最近影像中心
    for (int y = 0; y < mask.height; ++y)
    {
        double gy = mask.geoTransform[3] + y * mask.geoTransform[5];
        for (int x = 0; x < mask.width; ++x)
        {
            double gx = mask.geoTransform[0] + x * mask.geoTransform[1];
            int best = -1; double bestDist = 1e30;
            for (int i = 0; i < nImages; ++i)
            {
                if (gx < infos[i].xMin || gx > infos[i].xMax ||
                    gy < infos[i].yMin || gy > infos[i].yMax)
                    continue;
                double dx = gx - infos[i].cx, dy = gy - infos[i].cy;
                double dist = dx * dx + dy * dy;
                if (dist < bestDist) { bestDist = dist; best = i; }
            }
            if (best >= 0)
                mask.imageMasks[best][y * mask.width + x] = 1.0f;
        }
    }
    return mask;
}

bool SeamlineGenerator::exportSeamlinesToShapefile(
    const SeamlineMask& mask,
    const QStringList& imagePaths,
    const QString& shpPath)
{
    if (mask.width <= 0 || mask.height <= 0 || mask.imageMasks.isEmpty())
        return false;

    const int nImages = mask.imageMasks.size();
    const int pixels = mask.width * mask.height;

    // 1. 合并掩膜为单波段 label 栅格 (0=背景, 1..N=影像索引)
    //    使用 GDAL MEM 驱动在内存中创建, 避免临时文件
    GDALDriver* memDrv = GetGDALDriverManager()->GetDriverByName("MEM");
    if (!memDrv) return false;

    GDALDataset* labelDS = memDrv->Create("", mask.width, mask.height, 1,
                                           GDT_Int16, nullptr);
    if (!labelDS) return false;

    labelDS->SetGeoTransform(const_cast<double*>(mask.geoTransform));
    if (!mask.projection.isEmpty())
        labelDS->SetProjection(mask.projection.toUtf8().constData());

    std::vector<short> labelData(pixels, 0);
    for (int y = 0; y < mask.height; ++y)
    {
        for (int x = 0; x < mask.width; ++x)
        {
            int idx = y * mask.width + x;
            for (int i = 0; i < nImages; ++i)
            {
                if (mask.imageMasks[i][idx] > 0.5f)
                {
                    labelData[idx] = static_cast<short>(i + 1);
                    break;
                }
            }
        }
    }

    GDALRasterBand* band = labelDS->GetRasterBand(1);
    band->RasterIO(GF_Write, 0, 0, mask.width, mask.height,
                   labelData.data(), mask.width, mask.height,
                   GDT_Int16, 0, 0);
    band->SetNoDataValue(0);

    // 2. 先用 GDALPolygonize 在内存中生成多边形, 再提取边界为镶嵌线
    OGRSpatialReference sr;
    if (!mask.projection.isEmpty())
        sr.importFromWkt(mask.projection.toUtf8().constData());
    else
        sr.SetWellKnownGeogCS("WGS84");

    // 内存临时面图层
    GDALDriver* memVecDrv = GetGDALDriverManager()->GetDriverByName("Memory");
    if (!memVecDrv)
    {
        GDALClose(labelDS);
        return false;
    }
    GDALDataset* memDS = memVecDrv->Create("", 0, 0, 0, GDT_Unknown, nullptr);
    if (!memDS)
    {
        GDALClose(labelDS);
        return false;
    }
    OGRLayer* polyLayer = memDS->CreateLayer("polygons", &sr, wkbPolygon, nullptr);
    if (!polyLayer)
    {
        GDALClose(memDS);
        GDALClose(labelDS);
        return false;
    }
    OGRFieldDefn fid("img_idx", OFTInteger);
    polyLayer->CreateField(&fid);

    // GDALPolygonize → 内存面图层
    char* polyOpts[] = { nullptr };
    CPLErr e = GDALPolygonize(band, nullptr, polyLayer, 0, polyOpts, nullptr, nullptr);
    if (e != CE_None)
    {
        GDALClose(memDS);
        GDALClose(labelDS);
        return false;
    }

    // 3. 收集所有多边形几何与其影像索引
    QVector<OGRPolygon*> polys;
    QVector<int>          polyImgIdx;
    polyLayer->ResetReading();
    OGRFeature* polyFeat = nullptr;
    while ((polyFeat = polyLayer->GetNextFeature()) != nullptr)
    {
        OGRGeometry* geom = polyFeat->GetGeometryRef();
        if (!geom)
        {
            OGRFeature::DestroyFeature(polyFeat);
            continue;
        }

        OGRwkbGeometryType gtype = wkbFlatten(geom->getGeometryType());
        if (gtype == wkbPolygon)
        {
            OGRPolygon* cloned = static_cast<OGRPolygon*>(geom->clone());
            polys.append(cloned);
            polyImgIdx.append(polyFeat->GetFieldAsInteger("img_idx"));
        }
        else if (gtype == wkbMultiPolygon)
        {
            OGRMultiPolygon* mp = static_cast<OGRMultiPolygon*>(geom);
            for (int k = 0; k < mp->getNumGeometries(); ++k)
            {
                OGRPolygon* cloned = static_cast<OGRPolygon*>(mp->getGeometryRef(k)->clone());
                polys.append(cloned);
                polyImgIdx.append(polyFeat->GetFieldAsInteger("img_idx"));
            }
        }
        OGRFeature::DestroyFeature(polyFeat);
    }
    GDALClose(memDS);

    // 4. 创建输出线 Shapefile — 仅输出内部共享边 (镶嵌线)
    GDALDriver* shpDrv = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
    if (!shpDrv)
    {
        for (auto* p : polys) delete p;
        GDALClose(labelDS);
        return false;
    }

    GDALDataset* shpDS = shpDrv->Create(shpPath.toUtf8().constData(),
                                         0, 0, 0, GDT_Unknown, nullptr);
    if (!shpDS)
    {
        for (auto* p : polys) delete p;
        GDALClose(labelDS);
        return false;
    }

    OGRLayer* lineLayer = shpDS->CreateLayer("seamlines", &sr, wkbLineString, nullptr);
    if (!lineLayer)
    {
        GDALClose(shpDS);
        for (auto* p : polys) delete p;
        GDALClose(labelDS);
        return false;
    }

    OGRFieldDefn fldIdx("img_idx", OFTInteger);
    lineLayer->CreateField(&fldIdx);
    OGRFieldDefn fldPath("img_path", OFTString);
    fldPath.SetWidth(254);
    lineLayer->CreateField(&fldPath);
    OGRFieldDefn fldName("img_name", OFTString);
    fldName.SetWidth(254);
    lineLayer->CreateField(&fldName);

    // 5. 两两计算边界交集 — 交集即为内部镶嵌线 (剔除外侧包络线)
    const int nPolys = polys.size();
    for (int i = 0; i < nPolys; ++i)
    {
        OGRGeometry* boundaryI = polys[i]->getBoundary();
        if (!boundaryI) continue;

        for (int j = i + 1; j < nPolys; ++j)
        {
            // 同一影像的多边形之间不产生镶嵌线
            if (polyImgIdx[i] == polyImgIdx[j])
                continue;

            OGRGeometry* boundaryJ = polys[j]->getBoundary();
            if (!boundaryJ) continue;

            OGRGeometry* shared = boundaryI->Intersection(boundaryJ);
            if (!shared || shared->IsEmpty())
            {
                delete shared;
                continue;
            }

            OGRwkbGeometryType stype = wkbFlatten(shared->getGeometryType());
            if (stype == wkbLineString)
            {
                OGRFeature* feat = OGRFeature::CreateFeature(lineLayer->GetLayerDefn());
                feat->SetGeometry(shared);
                int i0 = polyImgIdx[i] - 1;  // 转 0-based
                int i1 = polyImgIdx[j] - 1;
                feat->SetField("img_idx", polyImgIdx[i]);
                if (i0 >= 0 && i0 < imagePaths.size())
                {
                    feat->SetField("img_path", imagePaths[i0].toUtf8().constData());
                    QFileInfo fi(imagePaths[i0]);
                    feat->SetField("img_name", fi.fileName().toUtf8().constData());
                }
                lineLayer->CreateFeature(feat);
                OGRFeature::DestroyFeature(feat);
            }
            else if (stype == wkbMultiLineString)
            {
                OGRMultiLineString* mls = static_cast<OGRMultiLineString*>(shared);
                for (int k = 0; k < mls->getNumGeometries(); ++k)
                {
                    OGRFeature* feat = OGRFeature::CreateFeature(lineLayer->GetLayerDefn());
                    feat->SetGeometry(mls->getGeometryRef(k));
                    feat->SetField("img_idx", polyImgIdx[i]);
                    int i0 = polyImgIdx[i] - 1;
                    if (i0 >= 0 && i0 < imagePaths.size())
                    {
                        feat->SetField("img_path", imagePaths[i0].toUtf8().constData());
                        QFileInfo fi(imagePaths[i0]);
                        feat->SetField("img_name", fi.fileName().toUtf8().constData());
                    }
                    lineLayer->CreateFeature(feat);
                    OGRFeature::DestroyFeature(feat);
                }
            }

            delete shared;
        }
    }

    for (auto* p : polys) delete p;
    GDALClose(shpDS);
    GDALClose(labelDS);
    return true;
}
