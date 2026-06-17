#include "GeometricServiceImpl.h"
#include "controllers/workers/GeometricWorker.h"
#include "controllers/WorkerManager.h"
#include <QThread>
#include <QDebug>

GeometricServiceImpl::GeometricServiceImpl(WorkerManager* workerManager,
                                           QObject* parent)
    : IGeometricService()
    , mWorkerManager(workerManager)
{
    setParent(parent);
}

void GeometricServiceImpl::execute(const GeometricCorrectionParams& params)
{
    qDebug() << "[GeometricService] execute:" << params.sourceImage
             << "mode:" << params.matchingMode
             << "model:" << params.modelType;

    if (mCurrentWorker)
    {
        mCurrentWorker->requestCancel();
        cleanupWorker();
    }

    auto* worker = new GeometricWorker(params);
    mCurrentWorker = worker;

    QThread* thread = mWorkerManager->allocateThread("Geometric");
    mCurrentThread = thread;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &GeometricWorker::process);
    connect(worker, &TaskWorker::progressChanged, this, &IGeometricService::progressChanged);
    connect(worker, &TaskWorker::finished, this, [this](bool success, const QString& path)
    {
        mRunning = false;
        emit finished(success, path);
        cleanupWorker();
    });
    connect(worker, &TaskWorker::errorOccurred, this, &IGeometricService::errorOccurred);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);

    mRunning = true;
    thread->start();
}

void GeometricServiceImpl::executeBatch(const QList<GeometricCorrectionParams>& batch)
{
    qDebug() << "[GeometricService] executeBatch:" << batch.size();
    for (const auto& p : batch)
        execute(p);
}

void GeometricServiceImpl::detectGcps(const GeometricCorrectionParams& params)
{
    qDebug() << "[GeometricService] detectGcps:" << params.sourceImage;
    // 在后台线程中运行 GCP 检测
    execute(params);
}

void GeometricServiceImpl::cancel()
{
    if (mCurrentWorker)
        mCurrentWorker->requestCancel();
    mRunning = false;
}

bool GeometricServiceImpl::isRunning() const
{
    return mRunning;
}

void GeometricServiceImpl::cleanupWorker()
{
    if (mCurrentThread)
    {
        mCurrentThread->quit();
        mCurrentThread->wait(5000);
        mCurrentThread = nullptr;
    }
    mCurrentWorker = nullptr;
}
