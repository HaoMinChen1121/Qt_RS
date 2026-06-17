#include "QualityMetricsEvaluator.h"
#include <gdal_priv.h>
#include <cmath>
#include <algorithm>
#include <QDebug>

static constexpr int SSIM_WIN = 8;

double QualityMetricsEvaluator::correlationCoefficient(const float* a, const float* b, int n)
{
    double ma = 0, mb = 0;
    for (int i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; }
    ma /= n; mb /= n;
    double num = 0, da = 0, db = 0;
    for (int i = 0; i < n; ++i)
    {
        double da_i = a[i] - ma, db_i = b[i] - mb;
        num += da_i * db_i;
        da += da_i * da_i;
        db += db_i * db_i;
    }
    return (da < 1e-12 || db < 1e-12) ? 0.0 : num / std::sqrt(da * db);
}

double QualityMetricsEvaluator::rmse(const float* a, const float* b, int n)
{
    double s = 0;
    for (int i = 0; i < n; ++i) { double d = a[i] - b[i]; s += d * d; }
    return std::sqrt(s / n);
}

double QualityMetricsEvaluator::averageGradient(const float* img, int w, int h)
{
    double sum = 0;
    int cnt = 0;
    for (int y = 1; y < h; ++y)
    {
        for (int x = 1; x < w; ++x)
        {
            double dx = img[y * w + x] - img[y * w + x - 1];
            double dy = img[y * w + x] - img[(y - 1) * w + x];
            sum += std::sqrt((dx * dx + dy * dy) / 2.0);
            ++cnt;
        }
    }
    return cnt > 0 ? sum / cnt : 0.0;
}

double QualityMetricsEvaluator::ssim(const float* a, const float* b, int w, int h)
{
    const double C1 = 6.5025, C2 = 58.5225; // (0.01*255)^2, (0.03*255)^2
    double ma = 0, mb = 0;
    int n = w * h;
    for (int i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; }
    ma /= n; mb /= n;
    double va = 0, vb = 0, cov = 0;
    for (int i = 0; i < n; ++i)
    {
        double da = a[i] - ma, db = b[i] - mb;
        va += da * da; vb += db * db; cov += da * db;
    }
    va /= n; vb /= n; cov /= n;
    double num = (2.0 * ma * mb + C1) * (2.0 * cov + C2);
    double den = (ma * ma + mb * mb + C1) * (va + vb + C2);
    return den < 1e-12 ? 0.0 : num / den;
}

FusionQualityMetrics QualityMetricsEvaluator::evaluate(
    const QString& fusedPath, const QString& msPath,
    const ImageFusionParams& params)
    {
    FusionQualityMetrics m;
    GDALAllRegister();

    GDALDataset* fusedDS = (GDALDataset*)GDALOpen(fusedPath.toUtf8(), GA_ReadOnly);
    GDALDataset* msDS = (GDALDataset*)GDALOpen(msPath.toUtf8(), GA_ReadOnly);
    if (!fusedDS || !msDS)
    {
        if (fusedDS) GDALClose(fusedDS);
        if (msDS) GDALClose(msDS);
        qWarning() << "[QualityMetrics] Failed to open fused or MS image";
        return m;
    }

    int fw = fusedDS->GetRasterXSize(), fh = fusedDS->GetRasterYSize();
    int mw = msDS->GetRasterXSize(), mh = msDS->GetRasterYSize();
    int fBands = fusedDS->GetRasterCount();
    int pixels = fw * fh;
    int msPixels = mw * mh;

    // 读第一波段做无参考指标
    float* fBand = new float[pixels];
    fusedDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, fw, fh, fBand, fw, fh, GDT_Float32, 0, 0);

    if (params.computeAverageGradient)
        m.averageGradient = averageGradient(fBand, fw, fh);

    // 参考型指标: 需要将 MS 上采样到 fused 分辨率
    float** msUpsampled = nullptr;
    if (params.computeCorrelationCoefficient || params.computeRMSE)
    {
        msUpsampled = new float*[fBands];
        float* msRaw = new float[msPixels];
        float xR = (float)mw / fw, yR = (float)mh / fh;

        double corrSum = 0;
        double rmseSum = 0;
        for (int b = 0; b < fBands && b < msDS->GetRasterCount(); ++b)
        {
            msUpsampled[b] = new float[pixels];
            msDS->GetRasterBand(b + 1)->RasterIO(GF_Read, 0, 0, mw, mh, msRaw, mw, mh, GDT_Float32, 0, 0);
            for (int y = 0; y < fh; ++y)
                for (int x = 0; x < fw; ++x)
                    msUpsampled[b][y * fw + x] = msRaw[(int)(y * yR) * mw + (int)(x * xR)];

            float* fb = new float[pixels];
            fusedDS->GetRasterBand(b + 1)->RasterIO(GF_Read, 0, 0, fw, fh, fb, fw, fh, GDT_Float32, 0, 0);

            if (params.computeCorrelationCoefficient)
                corrSum += correlationCoefficient(fb, msUpsampled[b], pixels);
            if (params.computeRMSE)
                rmseSum += rmse(fb, msUpsampled[b], pixels);
            delete[] fb;
        }
        if (params.computeCorrelationCoefficient)
            m.correlationCoefficient = corrSum / fBands;
        if (params.computeRMSE)
            m.rmse = rmseSum / fBands;

        delete[] msRaw;
        for (int b = 0; b < fBands; ++b) delete[] msUpsampled[b];
        delete[] msUpsampled;
    }

    if (params.computeSSIM)
    {
        float* msRaw = new float[msPixels];
        msDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, mw, mh, msRaw, mw, mh, GDT_Float32, 0, 0);
        float* msUp = new float[pixels];
        float xR = (float)mw / fw, yR = (float)mh / fh;
        for (int y = 0; y < fh; ++y)
            for (int x = 0; x < fw; ++x)
                msUp[y * fw + x] = msRaw[(int)(y * yR) * mw + (int)(x * xR)];
        m.ssim = ssim(fBand, msUp, fw, fh);
        delete[] msRaw; delete[] msUp;
    }

    delete[] fBand;
    GDALClose(fusedDS);
    GDALClose(msDS);

    qDebug() << "[QualityMetrics] CC:" << m.correlationCoefficient
             << "AG:" << m.averageGradient << "RMSE:" << m.rmse << "SSIM:" << m.ssim;
    return m;
}
