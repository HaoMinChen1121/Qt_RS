#ifndef GEOMETRICSERVICEIMPL_H
#define GEOMETRICSERVICEIMPL_H

#include "services/IGeometricService.h"

class WorkerManager;
class GeometricWorker;
class QThread;

class GeometricServiceImpl : public IGeometricService
{
    Q_OBJECT
public:
    explicit GeometricServiceImpl(WorkerManager* workerManager,
                                  QObject* parent = nullptr);

    void execute(const GeometricCorrectionParams& params) override;
    void executeBatch(const QList<GeometricCorrectionParams>& batch) override;
    void detectGcps(const GeometricCorrectionParams& params) override;
    void cancel() override;
    bool isRunning() const override;

private:
    void cleanupWorker();

    WorkerManager* mWorkerManager;
    bool mRunning = false;
    QThread* mCurrentThread = nullptr;
    GeometricWorker* mCurrentWorker = nullptr;
};

#endif // GEOMETRICSERVICEIMPL_H
