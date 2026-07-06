#ifndef RASTERCLIPSERVICEIMPL_H
#define RASTERCLIPSERVICEIMPL_H

#include "services/IRasterClipService.h"

class WorkerManager;
class QThread;
class RasterClipWorker;

class RasterClipServiceImpl : public IRasterClipService
{
    Q_OBJECT
public:
    explicit RasterClipServiceImpl(WorkerManager* workerManager, QObject* parent = nullptr);

    void execute(const RasterClipParams& params) override;
    void cancel() override;
    bool isRunning() const override;

private:
    void cleanupWorker();

    WorkerManager* mWorkerManager;
    bool mRunning = false;
    QThread* mCurrentThread = nullptr;
    RasterClipWorker* mCurrentWorker = nullptr;
};

#endif // RASTERCLIPSERVICEIMPL_H
