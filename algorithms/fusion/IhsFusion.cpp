#include "IhsFusion.h"
#include <gdal_priv.h>
#include <cmath>
#include <algorithm>
#include <QDebug>

/// 双线性上采样 MS 波段到 PAN 分辨率
static void upsampleBilinear(const float* src, int sw, int sh,
                              float* dst, int dw, int dh)
                              {
    float xRatio = (float)(sw - 1) / (dw > 1 ? dw - 1 : 1);
    float yRatio = (float)(sh - 1) / (dh > 1 ? dh - 1 : 1);
    for (int y = 0; y < dh; ++y)
    {
        float sy = y * yRatio;
        int sy0 = (int)sy;
        int sy1 = std::min(sy0 + 1, sh - 1);
        float wy = sy - sy0;
        for (int x = 0; x < dw; ++x)
        {
            float sx = x * xRatio;
            int sx0 = (int)sx;
            int sx1 = std::min(sx0 + 1, sw - 1);
            float wx = sx - sx0;
            float v00 = src[sy0 * sw + sx0], v10 = src[sy0 * sw + sx1];
            float v01 = src[sy1 * sw + sx0], v11 = src[sy1 * sw + sx1];
            dst[y * dw + x] = (1 - wy) * ((1 - wx) * v00 + wx * v10)
                            +     wy  * ((1 - wx) * v01 + wx * v11);
        }
    }
}

/// RGB → IHS: 使用 atan2 的稳定公式
/// I = (R+G+B)/3, S = sqrt(dr² + dg² + db²), H = atan2(√3(g-b), 2r-g-b)
static void rgbToIhs(float r, float g, float b, float& i, float& h, float& s)
{
    i = (r + g + b) / 3.0f;
    float dr = r - i, dg = g - i, db = b - i;
    s = std::sqrt(dr * dr + dg * dg + db * db);
    if (s < 1e-8f) { h = 0.0f; return; }
    h = std::atan2(1.73205081f * (dg - db), 2.0f * dr - dg - db);
    if (h < 0.0f) h += 2.0f * 3.14159265f;
}

/// IHS → RGB
static void ihsToRgb(float i, float h, float s, float& r, float& g, float& b)
{
    if (s < 1e-8f) { r = g = b = i; return; }
    float cosH = std::cos(h), sinH = std::sin(h);
    r = i + s * (2.0f * cosH) / 3.0f;
    g = i + s * (-cosH + 1.73205081f * sinH) / 3.0f;
    b = i + s * (-cosH - 1.73205081f * sinH) / 3.0f;
}

AlgorithmResult IhsFusion::fuse(const QString& panPath, const QString& msPath,
                                  const QString& outputPath,
                                  const ImageFusionParams& params,
                                  ProgressCallback progress)
                                  {
    Q_UNUSED(params);
    GDALAllRegister();

    GDALDataset* panDS = (GDALDataset*)GDALOpen(panPath.toUtf8(), GA_ReadOnly);
    if (!panDS) return {false, QStringLiteral("Failed to open pan image: ") + panPath};

    GDALDataset* msDS = (GDALDataset*)GDALOpen(msPath.toUtf8(), GA_ReadOnly);
    if (!msDS) { GDALClose(panDS); return {false, QStringLiteral("Failed to open MS image: ") + msPath}; }

    int panW = panDS->GetRasterXSize(), panH = panDS->GetRasterYSize();
    int msW  = msDS->GetRasterXSize(),  msH  = msDS->GetRasterYSize();
    int msBands = msDS->GetRasterCount();
    if (msBands < 3)
    {
        GDALClose(panDS); GDALClose(msDS);
        return {false, QStringLiteral("IHS requires at least 3 MS bands, got %1").arg(msBands)};
    }
    int useBands = std::min(3, msBands);
    int pixels = panW * panH;

    // 读取 PAN
    float* pan = new float[pixels];
    panDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, panW, panH, pan, panW, panH, GDT_Float32, 0, 0);

    if (progress && !progress(10, QStringLiteral("PAN loaded")))
    {
        delete[] pan; GDALClose(panDS); GDALClose(msDS);
        return {false, QStringLiteral("Cancelled")};
    }

    // 读取 MS 并用双线性上采样到 PAN 分辨率
    float** msUp = new float*[useBands];
    float* msRaw = new float[msW * msH];
    for (int b = 0; b < useBands; ++b)
    {
        msUp[b] = new float[pixels];
        msDS->GetRasterBand(b + 1)->RasterIO(GF_Read, 0, 0, msW, msH, msRaw, msW, msH, GDT_Float32, 0, 0);
        upsampleBilinear(msRaw, msW, msH, msUp[b], panW, panH);
    }
    delete[] msRaw;

    if (progress && !progress(40, QStringLiteral("MS upsampled")))
    {
        for (int b = 0; b < useBands; ++b) delete[] msUp[b];
        delete[] msUp; delete[] pan; GDALClose(panDS); GDALClose(msDS);
        return {false, QStringLiteral("Cancelled")};
    }

    // 计算 I 分量的均值和标准差 (从 MS), 直方图匹配 PAN
    double iSum = 0, iSum2 = 0;
    for (int i = 0; i < pixels; ++i)
    {
        float ii = (msUp[0][i] + msUp[1][i] + msUp[2][i]) / 3.0f;
        iSum += ii; iSum2 += ii * ii;
    }
    float iMean = (float)(iSum / pixels);
    float iStd  = (float)std::sqrt(std::max(0.0, iSum2 / pixels - (double)iMean * iMean));
    if (iStd < 1e-6f) iStd = 1.0f;

    double pSum = 0, pSum2 = 0;
    for (int i = 0; i < pixels; ++i) { pSum += pan[i]; pSum2 += pan[i] * pan[i]; }
    float pMean = (float)(pSum / pixels);
    float pStd  = (float)std::sqrt(std::max(0.0, pSum2 / pixels - (double)pMean * pMean));
    if (pStd < 1e-6f) pStd = 1.0f;

    float* panMatched = new float[pixels];
    for (int i = 0; i < pixels; ++i)
        panMatched[i] = (float)((pan[i] - pMean) * iStd / pStd + iMean);

    if (progress && !progress(60, QStringLiteral("IHS fusing...")))
    {
        delete[] panMatched;
        for (int b = 0; b < useBands; ++b) delete[] msUp[b];
        delete[] msUp; delete[] pan; GDALClose(panDS); GDALClose(msDS);
        return {false, QStringLiteral("Cancelled")};
    }

    // 逐像素 IHS 融合: MS→IHS, 用 panMatched 替换 I, 逆变换
    float** fused = new float*[useBands];
    for (int b = 0; b < useBands; ++b) fused[b] = new float[pixels];

    for (int i = 0; i < pixels; ++i)
    {
        float r = msUp[0][i], g = msUp[1][i], b = msUp[2][i];
        float ih, ii, is;
        rgbToIhs(r, g, b, ii, ih, is);
        // 低饱和度区域: 混合原始 I 分量以减少色调噪声
        float blend = std::min(1.0f, is / (iStd * 0.05f + 1e-8f));
        float newI = blend * panMatched[i] + (1.0f - blend) * ii;
        ihsToRgb(newI, ih, is, fused[0][i], fused[1][i], fused[2][i]);
    }
    delete[] panMatched;

    if (progress && !progress(80, QStringLiteral("Clamping and writing...")))
    {
        for (int b = 0; b < useBands; ++b) { delete[] msUp[b]; delete[] fused[b]; }
        delete[] msUp; delete[] fused; delete[] pan; GDALClose(panDS); GDALClose(msDS);
        return {false, QStringLiteral("Cancelled")};
    }

    // 统计输出范围并 clamp
    float outMin = 0.0f, outMax = 255.0f;
    {
        double totalMin = 1e30, totalMax = -1e30;
        for (int b = 0; b < useBands; ++b)
        {
            for (int i = 0; i < pixels; ++i)
            {
                if (fused[b][i] < totalMin) totalMin = fused[b][i];
                if (fused[b][i] > totalMax) totalMax = fused[b][i];
            }
        }
        if (totalMin < 0) outMin = 0.0f;
        outMax = (float)std::max(255.0, totalMax * 1.05);
    }
    for (int b = 0; b < useBands; ++b)
        for (int i = 0; i < pixels; ++i)
            fused[b][i] = std::clamp(fused[b][i], outMin, outMax);

    // 写入输出
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* outDS = driver->Create(outputPath.toUtf8(), panW, panH, useBands, GDT_Float32, nullptr);
    double geo[6];
    panDS->GetGeoTransform(geo);
    outDS->SetGeoTransform(geo);
    outDS->SetProjection(panDS->GetProjectionRef());
    for (int b = 0; b < useBands; ++b)
        outDS->GetRasterBand(b + 1)->RasterIO(GF_Write, 0, 0, panW, panH, fused[b], panW, panH, GDT_Float32, 0, 0);
    GDALClose(outDS);

    for (int b = 0; b < useBands; ++b) { delete[] msUp[b]; delete[] fused[b]; }
    delete[] msUp; delete[] fused; delete[] pan;
    GDALClose(panDS); GDALClose(msDS);

    if (progress) progress(100, QStringLiteral("IHS fusion complete"));
    return {true, outputPath};
}
