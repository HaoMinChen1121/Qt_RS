#ifndef IGEOMETRICSERVICE_H
#define IGEOMETRICSERVICE_H

#include "services/IProcessingService.h"
#include "domain/params/GeometricCorrectionParams.h"

class IGeometricService : public IProcessingService
{
    Q_OBJECT
public:
    virtual void execute(const GeometricCorrectionParams& params) = 0;
    virtual void executeBatch(const QList<GeometricCorrectionParams>& batch) = 0;
    /// 仅运行 GCP 检测，不执行校正
    virtual void detectGcps(const GeometricCorrectionParams& params) = 0;
};

#endif // IGEOMETRICSERVICE_H
