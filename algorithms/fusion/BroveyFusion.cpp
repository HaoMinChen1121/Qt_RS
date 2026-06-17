#include "BroveyFusion.h"
#include <gdal_priv.h>
#include <cmath>
#include <algorithm>
#include <QDebug>

AlgorithmResult BroveyFusion::fuse(const QString& panPath, const QString& msPath,
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

    // 读取 PAN
    float* pan = new float[pixels];
    panDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, panW, panH, pan, panW, panH, GDT_Float32, 0, 0);
    if (progress && !progress(10, QStringLiteral("PAN loaded")))
    {
        delete[] pan; GDALClose(panDS); GDALClose(msDS);
        return {false, QStringLiteral("Cancelled")};
    }

    // 读取并上采样 MS
    float** ms = new float*[msBands];
    float* raw = new float[msW * msH];
    float xRatio = (float)msW / panW, yRatio = (float)msH / panH;
    for (int b = 0; b < msBands; ++b)
    {
        ms[b] = new float[pixels];
        msDS->GetRasterBand(b + 1)->RasterIO(GF_Read, 0, 0, msW, msH, raw, msW, msH, GDT_Float32, 0, 0);
        for (int y = 0; y < panH; ++y)
        {
            int sy = (int)(y * yRatio);
            for (int x = 0; x < panW; ++x)
                ms[b][y * panW + x] = raw[sy * msW + (int)(x * xRatio)];
        }
    }
    delete[] raw;

    // 权重
    QList<double> weights = params.broveyBandWeights;
    while (weights.size() < msBands) weights.append(1.0);

    // Brovey: Fused_i = MS_i * PAN / sum(MS_j * w_j)
    float** fused = new float*[msBands];
    for (int b = 0; b < msBands; ++b) fused[b] = new float[pixels];

    for (int i = 0; i < pixels; ++i)
    {
        double denom = 0;
        for (int b = 0; b < msBands; ++b) denom += ms[b][i] * weights[b];
        if (denom < 1e-6f) denom = 1.0;
        for (int b = 0; b < msBands; ++b)
            fused[b][i] = ms[b][i] * pan[i] / (float)denom;
    }

    if (progress && !progress(60, QStringLiteral("Brovey done, writing...")))
    {
        for (int b = 0; b < msBands; ++b) { delete[] ms[b]; delete[] fused[b]; }
        delete[] ms; delete[] fused; delete[] pan;
        GDALClose(panDS); GDALClose(msDS);
        return {false, QStringLiteral("Cancelled")};
    }

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

    for (int b = 0; b < msBands; ++b) { delete[] ms[b]; delete[] fused[b]; }
    delete[] ms; delete[] fused; delete[] pan;
    GDALClose(panDS); GDALClose(msDS);

    if (progress) progress(100, QStringLiteral("Brovey fusion complete"));
    return {true, outputPath};
}
