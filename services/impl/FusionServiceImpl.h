#ifndef FUSIONSERVICEIMPL_H
#define FUSIONSERVICEIMPL_H

#include "services/IFusionService.h"

class IRasterReader;
class IRasterWriter;
class WorkerManager;
class FusionWorker;
class QThread;

class FusionServiceImpl : public IFusionService
{
    Q_OBJECT
public:
    FusionServiceImpl(IRasterReader* reader,
                      IRasterWriter* writer,
                      WorkerManager* workerManager,
                      QObject* parent = nullptr);

    void execute(const ImageFusionParams& params) override;
    void preview(const QString& panPath, const QString& msPath, const QString& method) override;
    void cancel() override;
    bool isRunning() const override;

private:
    void cleanupWorker();

    IRasterReader* mReader;
    IRasterWriter* mWriter;
    WorkerManager*  mWorkerManager;
    FusionWorker*   mCurrentWorker = nullptr;
    QThread*        mCurrentThread = nullptr;
    bool            mRunning = false;
};

#endif // FUSIONSERVICEIMPL_H
