#ifndef DEFINEPROJECTIONSERVICEIMPL_H
#define DEFINEPROJECTIONSERVICEIMPL_H
#include "services/IDefineProjectionService.h"

class WorkerManager;
class QThread;
class DefineProjectionWorker;

class DefineProjectionServiceImpl : public IDefineProjectionService
{
    Q_OBJECT
public:
    explicit DefineProjectionServiceImpl(WorkerManager* wm, QObject* parent = nullptr);
    void execute(const DefineProjectionParams& params) override;
    void cancel() override;
    bool isRunning() const override;
private:
    void cleanupWorker();
    WorkerManager* mWm; bool mRunning = false;
    QThread* mThread = nullptr; DefineProjectionWorker* mWorker = nullptr;
};
#endif
