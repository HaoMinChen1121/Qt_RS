#ifndef QUALITYMETRICSEVALUATOR_H
#define QUALITYMETRICSEVALUATOR_H

#include "domain/params/ImageFusionParams.h"
#include <QString>

class QualityMetricsEvaluator
{
public:
    /// 计算全部勾选的质量指标
    /// fusedPath: 融合结果, msPath: 原始多光谱(用于参考型指标)
    static FusionQualityMetrics evaluate(const QString& fusedPath,
                                          const QString& msPath,
                                          const ImageFusionParams& params);

private:
    static double correlationCoefficient(const float* a, const float* b, int n);
    static double averageGradient(const float* img, int w, int h);
    static double rmse(const float* a, const float* b, int n);
    static double ssim(const float* a, const float* b, int w, int h);
};

#endif // QUALITYMETRICSEVALUATOR_H
