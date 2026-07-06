#ifndef RASTERCLIPWORKER_H
#define RASTERCLIPWORKER_H

#include "controllers/TaskWorker.h"
#include "domain/params/RasterClipParams.h"

class RasterClipWorker : public TaskWorker
{
    Q_OBJECT
public:
    explicit RasterClipWorker(const RasterClipParams& params, QObject* parent = nullptr);
public slots:
    void process() override;
private:
    RasterClipParams mParams;
};

#endif // RASTERCLIPWORKER_H
