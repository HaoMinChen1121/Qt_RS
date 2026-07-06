#ifndef VECTORREPROJECTIONSERVICEIMPL_H
#define VECTORREPROJECTIONSERVICEIMPL_H
#include "services/IVectorReprojectionService.h"

class WorkerManager;
class QThread;
class VectorReprojectionWorker;

class VectorReprojectionServiceImpl : public IVectorReprojectionService
{
    Q_OBJECT
public:
    explicit VectorReprojectionServiceImpl(WorkerManager* wm, QObject* parent = nullptr);
    void execute(const VectorReprojectionParams& params) override;
    void cancel() override;
    bool isRunning() const override;
private:
    void cleanupWorker();
    WorkerManager* mWm; bool mRunning = false;
    QThread* mThread = nullptr; VectorReprojectionWorker* mWorker = nullptr;
};
#endif
