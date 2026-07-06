#ifndef RASTERREPROJECTIONWORKER_H
#define RASTERREPROJECTIONWORKER_H
#include "controllers/TaskWorker.h"
#include "domain/params/RasterReprojectionParams.h"

class RasterReprojectionWorker : public TaskWorker
{
    Q_OBJECT
public:
    explicit RasterReprojectionWorker(const RasterReprojectionParams& p, QObject* parent = nullptr);
public slots: void process() override;
private: RasterReprojectionParams mParams;
};
#endif
