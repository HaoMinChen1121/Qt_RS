#include "WaveletFusion.h"
#include <gdal_priv.h>
#include <cmath>
#include <algorithm>
#include <QDebug>

/// 1 层 Haar 离散小波分解: 输入 W×H, 输出 4 个子带 (LL, LH, HL, HH)
static void haarDecompose(const float* src, int w, int h,
                           float* ll, float* lh, float* hl, float* hh, int halfW, int halfH)
                           {
    float* tmpL = new float[w * h / 2]; // 行低通
    float* tmpH = new float[w * h / 2];
    float s2 = 0.70710678f; // 1/sqrt(2)
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < halfW; ++x)
        {
            float a = src[y * w + x * 2], b = src[y * w + x * 2 + 1];
            tmpL[y * halfW + x] = (a + b) * s2;
            tmpH[y * halfW + x] = (a - b) * s2;
        }
    }
    // 列分解
    for (int x = 0; x < halfW; ++x)
    {
        for (int y = 0; y < halfH; ++y)
        {
            float aL = tmpL[y * 2 * halfW + x], bL = tmpL[(y * 2 + 1) * halfW + x];
            float aH = tmpH[y * 2 * halfW + x], bH = tmpH[(y * 2 + 1) * halfW + x];
            ll[y * halfW + x] = (aL + bL) * s2;
            lh[y * halfW + x] = (aL - bL) * s2;
            hl[y * halfW + x] = (aH + bH) * s2;
            hh[y * halfW + x] = (aH - bH) * s2;
        }
    }
    delete[] tmpL; delete[] tmpH;
}

/// Haar 逆小波: LL + 细节 → 重建
static void haarReconstruct(const float* ll, const float* lh, const float* hl, const float* hh,
                              float* dst, int halfW, int halfH)
                              {
    float s2 = 0.70710678f;
    int w = halfW * 2;
    float* recL = new float[halfW * w]; // 列重建后行低频
    float* recH = new float[halfW * w];

    for (int x = 0; x < halfW; ++x)
    {
        for (int y = 0; y < halfH; ++y)
        {
            int idx = y * halfW + x;
            float aL = (ll[idx] + lh[idx]) * s2;
            float bL = (ll[idx] - lh[idx]) * s2;
            float aH = (hl[idx] + hh[idx]) * s2;
            float bH = (hl[idx] - hh[idx]) * s2;
            recL[(y * 2) * halfW + x] = aL;
            recL[(y * 2 + 1) * halfW + x] = bL;
            recH[(y * 2) * halfW + x] = aH;
            recH[(y * 2 + 1) * halfW + x] = bH;
        }
    }
    // 行重建
    for (int y = 0; y < w; ++y)
    {
        for (int x = 0; x < halfW; ++x)
        {
            float a = recL[y * halfW + x], b = recH[y * halfW + x];
            dst[y * w + x * 2]     = (a + b) * s2;
            dst[y * w + x * 2 + 1] = (a - b) * s2;
        }
    }
    delete[] recL; delete[] recH;
}

AlgorithmResult WaveletFusion::fuse(const QString& panPath, const QString& msPath,
                                      const QString& outputPath,
                                      const ImageFusionParams& params,
                                      ProgressCallback progress)
                                      {
    GDALAllRegister();
    GDALDataset* panDS = (GDALDataset*)GDALOpen(panPath.toUtf8(), GA_ReadOnly);
    if (!panDS) return {false, QStringLiteral("Failed to open pan")};
    GDALDataset* msDS = (GDALDataset*)GDALOpen(msPath.toUtf8(), GA_ReadOnly);
    if (!msDS) { GDALClose(panDS); return {false, QStringLiteral("Failed to open MS")}; }

    int panW = panDS->GetRasterXSize(), panH = panDS->GetRasterYSize();
    int msW  = msDS->GetRasterXSize(),  msH  = msDS->GetRasterYSize();
    int msBands = msDS->GetRasterCount();

    // 确保偶数尺寸 (Haar 需求)
    int useW = (panW % 2 == 0) ? panW : panW - 1;
    int useH = (panH % 2 == 0) ? panH : panH - 1;
    int halfW = useW / 2, halfH = useH / 2;
    int pixels = useW * useH, halfPixels = halfW * halfH;

    // 读取 PAN
    float* pan = new float[panW * panH];
    panDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, panW, panH, pan, panW, panH, GDT_Float32, 0, 0);
    // 裁剪到偶数尺寸
    float* panUse = new float[pixels];
    for (int y = 0; y < useH; ++y)
        for (int x = 0; x < useW; ++x)
            panUse[y * useW + x] = pan[y * panW + x];
    delete[] pan;

    if (progress && !progress(10, QStringLiteral("PAN loaded, wavelet decompose...")))
    {
        delete[] panUse; GDALClose(panDS); GDALClose(msDS);
        return {false, QStringLiteral("Cancelled")};
    }

    // PAN 小波分解
    float* panLL = new float[halfPixels], *panLH = new float[halfPixels];
    float* panHL = new float[halfPixels], *panHH = new float[halfPixels];
    haarDecompose(panUse, useW, useH, panLL, panLH, panHL, panHH, halfW, halfH);
    delete[] panUse;

    // 读取并上采样 MS
    float** fused = new float*[msBands];
    float* raw = new float[msW * msH];
    float xRatio = (float)msW / useW, yRatio = (float)msH / useH;
    float* tmpDecomp = new float[pixels];

    for (int b = 0; b < msBands; ++b)
    {
        fused[b] = new float[pixels];
        msDS->GetRasterBand(b + 1)->RasterIO(GF_Read, 0, 0, msW, msH, raw, msW, msH, GDT_Float32, 0, 0);
        for (int y = 0; y < useH; ++y)
            for (int x = 0; x < useW; ++x)
                tmpDecomp[y * useW + x] = raw[(int)(y * yRatio) * msW + (int)(x * xRatio)];

        // MS 小波分解
        float* msLL = new float[halfPixels], *msLH = new float[halfPixels];
        float* msHL = new float[halfPixels], *msHH = new float[halfPixels];
        haarDecompose(tmpDecomp, useW, useH, msLL, msLH, msHL, msHH, halfW, halfH);

        // 保留 MS 的 LL (光谱信息), 用 PAN 的细节 (LH/HL/HH) 替换
        haarReconstruct(msLL, panLH, panHL, panHH, fused[b], halfW, halfH);

        delete[] msLL; delete[] msLH; delete[] msHL; delete[] msHH;
    }
    delete[] tmpDecomp; delete[] raw;
    delete[] panLL; delete[] panLH; delete[] panHL; delete[] panHH;

    if (progress && !progress(70, QStringLiteral("Wavelet fusion done, writing...")))
    {
        for (int b = 0; b < msBands; ++b) delete[] fused[b];
        delete[] fused; GDALClose(panDS); GDALClose(msDS);
        return {false, QStringLiteral("Cancelled")};
    }

    // 输出
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* outDS = driver->Create(outputPath.toUtf8(), useW, useH, msBands, GDT_Float32, nullptr);
    // 复制 PAN 的地理参考 (调整尺寸)
    double geo[6];
    panDS->GetGeoTransform(geo);
    if (useW < panW) geo[0] += (panW - useW) / 2.0 * geo[1];
    if (useH < panH) geo[3] += (panH - useH) / 2.0 * geo[5];
    outDS->SetGeoTransform(geo);
    outDS->SetProjection(panDS->GetProjectionRef());
    for (int b = 0; b < msBands; ++b)
        outDS->GetRasterBand(b + 1)->RasterIO(GF_Write, 0, 0, useW, useH, fused[b], useW, useH, GDT_Float32, 0, 0);
    GDALClose(outDS);

    for (int b = 0; b < msBands; ++b) delete[] fused[b];
    delete[] fused;
    GDALClose(panDS); GDALClose(msDS);

    if (progress) progress(100, QStringLiteral("Wavelet fusion complete"));
    return {true, outputPath};
}
