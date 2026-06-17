#include "FusionServiceImpl.h"
#include "controllers/workers/FusionWorker.h"
#include "controllers/WorkerManager.h"
#include "dataaccess/IRasterReader.h"
#include "dataaccess/IRasterWriter.h"
#include <QThread>
#include <QDebug>

FusionServiceImpl::FusionServiceImpl(IRasterReader* reader,
                                       IRasterWriter* writer,
                                       WorkerManager* workerManager,
                                       QObject* parent)
    : IFusionService()
    , mReader(reader)
    , mWriter(writer)
    , mWorkerManager(workerManager)
    {
    setParent(parent);
}

void FusionServiceImpl::execute(const ImageFusionParams& params)
{
    qDebug() << "[FusionService] execute: algo=" << params.algorithm
             << "pan=" << params.panchromaticImage;

    if (mCurrentWorker)
    {
        mCurrentWorker->requestCancel();
        cleanupWorker();
    }

    auto* worker = new FusionWorker(mReader, mWriter, params);
    mCurrentWorker = worker;

    QThread* thread = mWorkerManager->allocateThread(QStringLiteral("Fusion"));
    mCurrentThread = thread;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &FusionWorker::process);
    connect(worker, &TaskWorker::progressChanged, this, &IFusionService::progressChanged);
    connect(worker, &TaskWorker::finished, this, [this](bool success, const QString& path)
    {
        mRunning = false;
        emit finished(success, path);
        cleanupWorker();
    });
    connect(worker, &TaskWorker::errorOccurred, this, &IFusionService::errorOccurred);
    connect(worker, &FusionWorker::qualityMetricsReady, this, &IFusionService::qualityMetricsReady);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);

    mRunning = true;
    thread->start();
}

void FusionServiceImpl::preview(const QString& panPath, const QString& msPath, const QString& method)
{
    qDebug() << "[FusionService] preview:" << method << panPath << msPath;
}

void FusionServiceImpl::cancel()
{
    qDebug() << "[FusionService] cancel";
    if (mCurrentWorker)
        mCurrentWorker->requestCancel();
    mRunning = false;
}

bool FusionServiceImpl::isRunning() const { return mRunning; }

void FusionServiceImpl::cleanupWorker()
{
    if (mCurrentThread)
    {
        mCurrentThread->quit();
        mCurrentThread->wait(5000);
        mCurrentThread = nullptr;
    }
    mCurrentWorker = nullptr;
}
