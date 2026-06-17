#ifndef FUSIONWORKER_H
#define FUSIONWORKER_H

#include "controllers/TaskWorker.h"
#include "domain/params/ImageFusionParams.h"

class IRasterReader;
class IRasterWriter;

class FusionWorker : public TaskWorker
{
    Q_OBJECT
public:
    FusionWorker(IRasterReader* reader,
                 IRasterWriter* writer,
                 const ImageFusionParams& params,
                 QObject* parent = nullptr);

public slots:
    void process() override;

signals:
    void qualityMetricsReady(const FusionQualityMetrics& metrics);

private:
    IRasterReader* mReader;
    IRasterWriter* mWriter;
    ImageFusionParams mParams;
};

#endif // FUSIONWORKER_H
