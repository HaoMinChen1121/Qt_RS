#include "HpfFusion.h"
#include <gdal_priv.h>
#include <cmath>
#include <algorithm>
#include <QDebug>

/// 均值滤波 (box blur)
static void boxFilter(const float* src, float* dst, int w, int h, int kernel)
{
    int half = kernel / 2;
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            double sum = 0; int cnt = 0;
            for (int ky = -half; ky <= half; ++ky)
            {
                int yy = std::clamp(y + ky, 0, h - 1);
                for (int kx = -half; kx <= half; ++kx)
                {
                    sum += src[yy * w + std::clamp(x + kx, 0, w - 1)];
                    ++cnt;
                }
            }
            dst[y * w + x] = (float)(sum / cnt);
        }
    }
}

AlgorithmResult HpfFusion::fuse(const QString& panPath, const QString& msPath,
                                  const QString& outputPath,
                                  const ImageFusionParams& params,
                                  ProgressCallback progress)
                                  {
    GDALAllRegister();

    GDALDataset* panDS = (GDALDataset*)GDALOpen(panPath.toUtf8(), GA_ReadOnly);
    if (!panDS) return {false, QStringLiteral("Failed to open pan: ") + panPath};
    GDALDataset* msDS = (GDALDataset*)GDALOpen(msPath.toUtf8(), GA_ReadOnly);
    if (!msDS) { GDALClose(panDS); return {false, QStringLiteral("Failed to open MS: ") + msPath}; }

    int panW = panDS->GetRasterXSize(), panH = panDS->GetRasterYSize();
    int msW  = msDS->GetRasterXSize(),  msH  = msDS->GetRasterYSize();
    int msBands = msDS->GetRasterCount();
    int pixels = panW * panH;
    int kernelSize = params.hpfKernelSize > 0 ? params.hpfKernelSize : 5;
    float weight = (float)(params.hpfWeight > 0 ? params.hpfWeight : 0.5);

    float* pan = new float[pixels];
    panDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, panW, panH, pan, panW, panH, GDT_Float32, 0, 0);
    if (progress && !progress(10, QStringLiteral("PAN loaded")))
    {
        delete[] pan; GDALClose(panDS); GDALClose(msDS);
        return {false, QStringLiteral("Cancelled")};
    }

    // 均值滤波得到低通 PAN
    float* panLow = new float[pixels];
    boxFilter(pan, panLow, panW, panH, kernelSize);
    // 细节 = PAN - 低通PAN
    float* detail = new float[pixels];
    for (int i = 0; i < pixels; ++i) detail[i] = pan[i] - panLow[i];
    delete[] panLow;

    if (progress && !progress(30, QStringLiteral("HPF detail extracted")))
    {
        delete[] pan; delete[] detail; GDALClose(panDS); GDALClose(msDS);
        return {false, QStringLiteral("Cancelled")};
    }

    // 上采样 MS, 注入细节
    float** fused = new float*[msBands];
    float* raw = new float[msW * msH];
    float xRatio = (float)msW / panW, yRatio = (float)msH / panH;

    for (int b = 0; b < msBands; ++b)
    {
        fused[b] = new float[pixels];
        msDS->GetRasterBand(b + 1)->RasterIO(GF_Read, 0, 0, msW, msH, raw, msW, msH, GDT_Float32, 0, 0);
        for (int y = 0; y < panH; ++y)
        {
            int sy = (int)(y * yRatio);
            for (int x = 0; x < panW; ++x)
                fused[b][y * panW + x] = raw[sy * msW + (int)(x * xRatio)] + weight * detail[y * panW + x];
        }
    }
    delete[] raw; delete[] detail; delete[] pan;

    // 输出
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* outDS = driver->Create(outputPath.toUtf8(), panW, panH, msBands, GDT_Float32, nullptr);
    double geo[6];
    panDS->GetGeoTransform(geo);
    outDS->SetGeoTransform(geo);
    outDS->SetProjection(panDS->GetProjectionRef());
    for (int b = 0; b < msBands; ++b)
        outDS->GetRasterBand(b + 1)->RasterIO(GF_Write, 0, 0, panW, panH, fused[b], panW, panH, GDT_Float32, 0, 0);
    GDALClose(outDS);

    for (int b = 0; b < msBands; ++b) delete[] fused[b];
    delete[] fused;
    GDALClose(panDS); GDALClose(msDS);

    if (progress) progress(100, QStringLiteral("HPF fusion complete"));
    return {true, outputPath};
}
