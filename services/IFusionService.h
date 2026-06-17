#ifndef IFUSIONSERVICE_H
#define IFUSIONSERVICE_H

#include "services/IProcessingService.h"
#include "domain/params/ImageFusionParams.h"

class IFusionService : public IProcessingService
{
    Q_OBJECT
public:
    virtual void execute(const ImageFusionParams& params) = 0;
    virtual void preview(const QString& panPath, const QString& msPath, const QString& method) = 0;

signals:
    void qualityMetricsReady(const FusionQualityMetrics& metrics);
};

#endif // IFUSIONSERVICE_H
