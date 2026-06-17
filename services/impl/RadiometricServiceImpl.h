#ifndef RADIOMETRICSERVICEIMPL_H
#define RADIOMETRICSERVICEIMPL_H

#include "services/IRadiometricService.h"

class IRasterReader;
class IRasterWriter;
class WorkerManager;
class RadiometricWorker;
class QThread;

class RadiometricServiceImpl : public IRadiometricService
{
    Q_OBJECT
public:
    explicit RadiometricServiceImpl(IRasterReader* reader,
                                     IRasterWriter* writer,
                                     WorkerManager* workerManager,
                                     QObject* parent = nullptr);

    void execute(const RadiometricCorrectionParams& params) override;
    void executeBatch(const QList<RadiometricCorrectionParams>& batch) override;
    void cancel() override;
    bool isRunning() const override;

private:
    void cleanupWorker();

    IRasterReader* mReader;
    IRasterWriter* mWriter;
    WorkerManager* mWorkerManager;

    bool mRunning = false;
    QThread* mCurrentThread = nullptr;
    RadiometricWorker* mCurrentWorker = nullptr;
};

#endif // RADIOMETRICSERVICEIMPL_H
