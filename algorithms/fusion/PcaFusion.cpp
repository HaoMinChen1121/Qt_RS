#include "PcaFusion.h"
#include <gdal_priv.h>
#include <cmath>
#include <algorithm>
#include <QDebug>

/// 对称矩阵 Jacobi 特征分解 (n 较小, n ≤ 12)
static void jacobiEigen(const double* A, int n, double* eigenVals, double* eigenVecs)
{
    // A: n×n 对称矩阵 (copy), eigenVecs: n×n 输出 (每列一个特征向量)
    double* a = new double[n * n];
    for (int i = 0; i < n * n; ++i) a[i] = A[i];
    // 初始化特征向量为单位矩阵
    for (int i = 0; i < n; ++i)
    {
        for (int j = 0; j < n; ++j) eigenVecs[i * n + j] = (i == j) ? 1.0 : 0.0;
    }

    const int maxIter = 100;
    for (int iter = 0; iter < maxIter; ++iter)
    {
        // 找最大非对角元
        int p = 0, q = 1;
        double maxOff = 0;
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j)
                if (std::abs(a[i * n + j]) > maxOff) { maxOff = std::abs(a[i * n + j]); p = i; q = j; }
        if (maxOff < 1e-12) break;

        // Jacobi 旋转
        double theta = 0.5 * std::atan2(2.0 * a[p * n + q], a[p * n + p] - a[q * n + q]);
        double c = std::cos(theta), s = std::sin(theta);

        // 更新 A
        double app = a[p * n + p], aqq = a[q * n + q], apq = a[p * n + q];
        a[p * n + p] = c * c * app + 2 * s * c * apq + s * s * aqq;
        a[q * n + q] = s * s * app - 2 * s * c * apq + c * c * aqq;
        a[p * n + q] = a[q * n + p] = 0.0;
        for (int i = 0; i < n; ++i)
        {
            if (i != p && i != q)
            {
                double aip = a[i * n + p], aiq = a[i * n + q];
                a[i * n + p] = a[p * n + i] = c * aip + s * aiq;
                a[i * n + q] = a[q * n + i] = -s * aip + c * aiq;
            }
        }
        // 更新特征向量
        for (int i = 0; i < n; ++i)
        {
            double vip = eigenVecs[i * n + p], viq = eigenVecs[i * n + q];
            eigenVecs[i * n + p] = c * vip + s * viq;
            eigenVecs[i * n + q] = -s * vip + c * viq;
        }
    }
    for (int i = 0; i < n; ++i) eigenVals[i] = a[i * n + i];
    delete[] a;
}

AlgorithmResult PcaFusion::fuse(const QString& panPath, const QString& msPath,
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
    int nPC = std::min(params.pcaComponentCount > 0 ? params.pcaComponentCount : msBands, msBands);

    // 读取 PAN
    float* pan = new float[pixels];
    panDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, panW, panH, pan, panW, panH, GDT_Float32, 0, 0);
    if (progress && !progress(10, QStringLiteral("PAN loaded")))
    {
        delete[] pan; GDALClose(panDS); GDALClose(msDS); return {false, QStringLiteral("Cancelled")};
    }

    // 采样提取部分像素做 PCA (加速协方差计算)
    int sampleStep = std::max(1, pixels / 50000);
    int nSamples = pixels / sampleStep;

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

    // 计算均值
    double* means = new double[msBands]{};
    for (int b = 0; b < msBands; ++b)
    {
        for (int i = 0; i < nSamples; ++i)
            means[b] += ms[b][i * sampleStep];
        means[b] /= nSamples;
    }

    // 协方差矩阵
    double* cov = new double[msBands * msBands]{};
    for (int b1 = 0; b1 < msBands; ++b1)
        for (int b2 = b1; b2 < msBands; ++b2)
        {
            double s = 0;
            for (int i = 0; i < nSamples; ++i)
                s += (ms[b1][i * sampleStep] - means[b1]) * (ms[b2][i * sampleStep] - means[b2]);
            cov[b1 * msBands + b2] = cov[b2 * msBands + b1] = s / (nSamples - 1);
        }

    double* eigenVals = new double[msBands];
    double* eigenVecs = new double[msBands * msBands];
    jacobiEigen(cov, msBands, eigenVals, eigenVecs);
    // 按特征值降序重排
    int* order = new int[msBands];
    for (int i = 0; i < msBands; ++i) order[i] = i;
    std::sort(order, order + msBands, [&](int a, int b) { return eigenVals[a] > eigenVals[b]; });

    if (progress && !progress(40, QStringLiteral("PCA decomposition done")))
    {
        delete[] pan; delete[] means; delete[] cov; delete[] eigenVals; delete[] eigenVecs; delete[] order;
        for (int b = 0; b < msBands; ++b) delete[] ms[b];
        delete[] ms; GDALClose(panDS); GDALClose(msDS); return {false, QStringLiteral("Cancelled")};
    }

    // 直方图匹配 PAN 到 PC1
    double pc1Sum = 0, pc1Sum2 = 0;
    float* pc1 = new float[pixels];
    int ev0 = order[0]; // 最大特征向量的列索引
    for (int i = 0; i < pixels; ++i)
    {
        double v = 0;
        for (int b = 0; b < msBands; ++b)
            v += (ms[b][i] - means[b]) * eigenVecs[b * msBands + ev0];
        pc1[i] = (float)v;
        pc1Sum += v; pc1Sum2 += v * v;
    }
    float pc1Mean = (float)(pc1Sum / pixels);
    float pc1Std  = (float)std::sqrt(pc1Sum2 / pixels - pc1Mean * pc1Mean);
    // Match PAN to PC1
    double panSum = 0, panSum2 = 0;
    for (int i = 0; i < pixels; ++i) { panSum += pan[i]; panSum2 += pan[i] * pan[i]; }
    float panMean = (float)(panSum / pixels);
    float panStd  = (float)std::sqrt(panSum2 / pixels - panMean * panMean);
    float* panMatched = new float[pixels];
    for (int i = 0; i < pixels; ++i)
        panMatched[i] = (float)((pan[i] - panMean) * pc1Std / (panStd + 1e-6f) + pc1Mean);
    delete[] pc1;

    // PCA 逆变换 (用 PAN 替换 PC1)
    float** fused = new float*[msBands];
    for (int b = 0; b < msBands; ++b) fused[b] = new float[pixels];

    for (int i = 0; i < pixels; ++i)
    {
        for (int b = 0; b < msBands; ++b)
        {
            double v = 0;
            // PC1 用 PAN 替换
            v += panMatched[i] * eigenVecs[b * msBands + ev0];
            // 其余 PC 保留
            for (int p = 1; p < nPC && p < msBands; ++p)
            {
                int evP = order[p];
                double pc = 0;
                for (int bb = 0; bb < msBands; ++bb)
                    pc += (ms[bb][i] - means[bb]) * eigenVecs[bb * msBands + evP];
                v += pc * eigenVecs[b * msBands + evP];
            }
            fused[b][i] = (float)(v + means[b]);
        }
    }
    delete[] panMatched;

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
    delete[] means; delete[] cov; delete[] eigenVals; delete[] eigenVecs; delete[] order;
    GDALClose(panDS); GDALClose(msDS);

    if (progress) progress(100, QStringLiteral("PCA fusion complete"));
    return {true, outputPath};
}
