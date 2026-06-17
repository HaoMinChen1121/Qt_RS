#include "SeamlineGenerator.h"
#include <gdal_priv.h>
#include <ogr_srs_api.h>
#include <cmath>
#include <algorithm>
#include <QDebug>

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

    mask.width  = (int)((xMax - xMin) / resolution + 0.5);
    mask.height = (int)((yMax - yMin) / resolution + 0.5);
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
