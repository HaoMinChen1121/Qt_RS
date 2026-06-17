#include "MosaicServiceImpl.h"
#include "controllers/workers/MosaicWorker.h"
#include "controllers/WorkerManager.h"
#include <QThread>
#include <QDebug>

MosaicServiceImpl::MosaicServiceImpl(WorkerManager* workerManager,
                                       QObject* parent)
    : IMosaicService()
    , mWorkerManager(workerManager)
    {
    setParent(parent);
}

void MosaicServiceImpl::execute(const MosaicParams& params)
{
    qDebug() << "[MosaicService] execute: images=" << params.inputImages.size()
             << "balance=" << params.colorBalanceMethod;

    if (mCurrentWorker)
    {
        mCurrentWorker->requestCancel();
        cleanupWorker();
    }

    auto* worker = new MosaicWorker(params);
    mCurrentWorker = worker;

    QThread* thread = mWorkerManager->allocateThread(QStringLiteral("Mosaic"));
    mCurrentThread = thread;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &MosaicWorker::process);
    connect(worker, &TaskWorker::progressChanged,
            this, &IMosaicService::progressChanged);
    connect(worker, &TaskWorker::finished, this,
        [this](bool success, const QString& path)
        {
            mRunning = false;
            emit finished(success, path);
            cleanupWorker();
        });
    connect(worker, &TaskWorker::errorOccurred,
            this, &IMosaicService::errorOccurred);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);

    mRunning = true;
    thread->start();
}

void MosaicServiceImpl::previewSeamlines(const QStringList& imagePaths)
{
    qDebug() << "[MosaicService] previewSeamlines:" << imagePaths.size();
    emit seamlinePreviewReady(QString());
}

void MosaicServiceImpl::cancel()
{
    qDebug() << "[MosaicService] cancel";
    if (mCurrentWorker)
        mCurrentWorker->requestCancel();
    mRunning = false;
}

bool MosaicServiceImpl::isRunning() const { return mRunning; }

void MosaicServiceImpl::cleanupWorker()
{
    if (mCurrentThread)
    {
        mCurrentThread->quit();
        mCurrentThread->wait(5000);
        mCurrentThread = nullptr;
    }
    mCurrentWorker = nullptr;
}
