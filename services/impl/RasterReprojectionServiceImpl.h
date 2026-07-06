#ifndef RASTERREPROJECTIONSERVICEIMPL_H
#define RASTERREPROJECTIONSERVICEIMPL_H
#include "services/IRasterReprojectionService.h"

class WorkerManager;
class QThread;
class RasterReprojectionWorker;

class RasterReprojectionServiceImpl : public IRasterReprojectionService
{
    Q_OBJECT
public:
    explicit RasterReprojectionServiceImpl(WorkerManager* wm, QObject* parent = nullptr);
    void execute(const RasterReprojectionParams& params) override;
    void cancel() override;
    bool isRunning() const override;
private:
    void cleanupWorker();
    WorkerManager* mWm; bool mRunning = false;
    QThread* mThread = nullptr; RasterReprojectionWorker* mWorker = nullptr;
};
#endif
