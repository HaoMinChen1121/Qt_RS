#include "GramSchmidtFusion.h"
#include <gdal_priv.h>
#include <cmath>
#include <algorithm>
#include <QDebug>

AlgorithmResult GramSchmidtFusion::fuse(const QString& panPath, const QString& msPath,
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
    int pixels = panW * panH;

    // 读取 PAN
    float* pan = new float[pixels];
    panDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, panW, panH, pan, panW, panH, GDT_Float32, 0, 0);
    if (progress && !progress(10, QStringLiteral("PAN loaded")))
    {
        delete[] pan; GDALClose(panDS); GDALClose(msDS); return {false, QStringLiteral("Cancelled")};
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
            for (int x = 0; x < panW; ++x)
                ms[b][y * panW + x] = raw[(int)(y * yRatio) * msW + (int)(x * xRatio)];
    }
    delete[] raw;

    // 模拟低分辨率 PAN: 各波段均值
    float* simPan = new float[pixels];
    double simMean = 0, simStd = 0;
    for (int i = 0; i < pixels; ++i)
    {
        double s = 0;
        for (int b = 0; b < msBands; ++b) s += ms[b][i];
        simPan[i] = (float)(s / msBands);
        simMean += simPan[i];
    }
    simMean /= pixels;
    double simSum2 = 0;
    for (int i = 0; i < pixels; ++i) simSum2 += (simPan[i] - simMean) * (simPan[i] - simMean);
    simStd = std::sqrt(simSum2 / pixels);

    // 直方图匹配 PAN 到模拟 PAN
    double panMean = 0, panSum2 = 0;
    for (int i = 0; i < pixels; ++i) { panMean += pan[i]; panSum2 += pan[i] * pan[i]; }
    panMean /= pixels;
    double panStd = std::sqrt(panSum2 / pixels - panMean * panMean);
    float* panAdj = new float[pixels];
    for (int i = 0; i < pixels; ++i)
        panAdj[i] = (float)((pan[i] - panMean) * simStd / (panStd + 1e-6) + simMean);

    if (progress && !progress(40, QStringLiteral("GS: simulated PAN ready")))
    {
        delete[] pan; delete[] panAdj; delete[] simPan;
        for (int b = 0; b < msBands; ++b) delete[] ms[b];
        delete[] ms; GDALClose(panDS); GDALClose(msDS); return {false, QStringLiteral("Cancelled")};
    }

    // Gram-Schmidt 正交化: 第 0 个 = simPan, 其余 = MS bands
    // GS forward
    int nVecs = msBands + 1;
    float** gs = new float*[nVecs]; // gs[0] = simPan, gs[1..] = ms
    gs[0] = simPan;
    for (int b = 0; b < msBands; ++b) gs[b + 1] = ms[b];

    // 正交化 (Modified Gram-Schmidt)
    for (int k = 0; k < nVecs; ++k)
    {
        // 归一化
        double normSq = 0;
        for (int i = 0; i < pixels; ++i) normSq += gs[k][i] * gs[k][i];
        double invNorm = 1.0 / std::sqrt(normSq + 1e-30);
        for (int i = 0; i < pixels; ++i) gs[k][i] = (float)(gs[k][i] * invNorm);

        // 从后续向量中减去投影
        for (int j = k + 1; j < nVecs; ++j)
        {
            double dot = 0;
            for (int i = 0; i < pixels; ++i) dot += gs[k][i] * gs[j][i];
            for (int i = 0; i < pixels; ++i) gs[j][i] = (float)(gs[j][i] - dot * gs[k][i]);
        }
    }

    // 用 PAN 替换第 0 个 GS 分量
    delete[] gs[0];
    gs[0] = panAdj;
    // 重新正交化: gs[0] 归一化, 其他分量减去与 gs[0] 的投影后逆变换
    double norm0 = 0;
    for (int i = 0; i < pixels; ++i) norm0 += gs[0][i] * gs[0][i];
    double invN0 = 1.0 / std::sqrt(norm0 + 1e-30);
    for (int i = 0; i < pixels; ++i) gs[0][i] = (float)(gs[0][i] * invN0);

    // 对于原始 GS 矩阵, 我们存储了正交基后各原始波段向量的系数
    // 简化: 用正交基逆推 (GS 逆变换 = 转置加法)
    for (int b = 0; b < msBands; ++b)
    {
        // 用新的 gs[0] 替换旧的, 但保持和各后续分量的正交关系
        // 简化实现: 对各 MS 波段加回 gs[0] 的影响 (均值项)
        double dotB = 0;
        for (int i = 0; i < pixels; ++i) dotB += gs[0][i] * gs[b + 1][i];
        for (int i = 0; i < pixels; ++i)
            ms[b][i] = ms[b][i] + gs[0][i] * (panAdj[i] - simPan[i]); // 简化的注入
    }

    // 用调整后的 PAN 替换 simPan 分量: 直接对各波段注入 PAN 细节
    float* detail = new float[pixels];
    for (int i = 0; i < pixels; ++i) detail[i] = panAdj[i] - simPan[i];

    // 输出: 原始 MS 上采样 + 按比例注入 PAN 细节
    // 计算每个波段的注入权重 (基于和 simPan 的相关性)
    double* weight = new double[msBands];
    for (int b = 0; b < msBands; ++b)
    {
        double num = 0, den = 0;
        for (int i = 0; i < pixels; ++i)
        {
            num += ms[b][i] * simPan[i];
            den += simPan[i] * simPan[i];
        }
        weight[b] = num / (den + 1e-30);
    }

    for (int b = 0; b < msBands; ++b)
        for (int i = 0; i < pixels; ++i)
            ms[b][i] += (float)(weight[b] * detail[i]);

    delete[] detail; delete[] weight;

    // 输出
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* outDS = driver->Create(outputPath.toUtf8(), panW, panH, msBands, GDT_Float32, nullptr);
    double geo[6];
    panDS->GetGeoTransform(geo);
    outDS->SetGeoTransform(geo);
    outDS->SetProjection(panDS->GetProjectionRef());
    for (int b = 0; b < msBands; ++b)
        outDS->GetRasterBand(b + 1)->RasterIO(GF_Write, 0, 0, panW, panH, ms[b], panW, panH, GDT_Float32, 0, 0);
    GDALClose(outDS);

    for (int b = 0; b < msBands; ++b) delete[] ms[b];
    delete[] ms; delete[] gs; delete[] pan; delete[] panAdj;
    GDALClose(panDS); GDALClose(msDS);

    if (progress) progress(100, QStringLiteral("Gram-Schmidt fusion complete"));
    return {true, outputPath};
}
