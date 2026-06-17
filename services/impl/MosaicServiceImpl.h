#ifndef MOSAICSERVICEIMPL_H
#define MOSAICSERVICEIMPL_H

#include "services/IMosaicService.h"

class WorkerManager;
class MosaicWorker;
class QThread;

class MosaicServiceImpl : public IMosaicService
{
    Q_OBJECT
public:
    explicit MosaicServiceImpl(WorkerManager* workerManager,
                                QObject* parent = nullptr);

    void execute(const MosaicParams& params) override;
    void previewSeamlines(const QStringList& imagePaths) override;
    void cancel() override;
    bool isRunning() const override;

private:
    void cleanupWorker();

    WorkerManager* mWorkerManager;
    MosaicWorker*  mCurrentWorker = nullptr;
    QThread*       mCurrentThread = nullptr;
    bool           mRunning = false;
};

#endif // MOSAICSERVICEIMPL_H
