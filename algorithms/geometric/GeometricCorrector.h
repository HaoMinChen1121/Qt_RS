#ifndef GEOMETRICCORRECTOR_H
#define GEOMETRICCORRECTOR_H

#include "domain/params/GeometricCorrectionParams.h"
#include "algorithms/common/AlgorithmResult.h"
#include "algorithms/common/ProgressCallback.h"

class GeometricCorrector
{
public:
    /// 执行完整几何校正流程
    GeometricResult correct(const GeometricCorrectionParams& params,
                            ProgressCallback progress = nullptr);

    /// 仅执行 GCP 匹配（不校正），返回匹配到的 GCP 列表
    QVector<Gcp> detectGcps(const GeometricCorrectionParams& params);

private:
    /// 从校正方向拟合模型（ref→src）
    GcpModel fitCorrectionModel(const QVector<Gcp>& gcps,
                                const QString& modelType);

    /// 计算输出影像的地理范围
    bool computeOutputExtent(const QString& srcImage,
                             const QString& refImage,
                             const GcpModel& forwardModel,
                             const GeometricCorrectionParams& params,
                             double extent[4]);
};

#endif // GEOMETRICCORRECTOR_H
