#include "RasterClipServiceImpl.h"
#include "controllers/WorkerManager.h"
#include "controllers/workers/RasterClipWorker.h"
#include <QThread>

RasterClipServiceImpl::RasterClipServiceImpl(WorkerManager* workerManager, QObject* parent)
    : IRasterClipService()
    , mWorkerManager(workerManager)
{
    setParent(parent);
}

void RasterClipServiceImpl::execute(const RasterClipParams& params)
{
    if (mRunning && mCurrentWorker) {
        mCurrentWorker->requestCancel();
        cleanupWorker();
    }

    mCurrentWorker = new RasterClipWorker(params);
    mCurrentThread = mWorkerManager->allocateThread(QStringLiteral("RasterClip"));

    mCurrentWorker->moveToThread(mCurrentThread);

    connect(mCurrentThread, &QThread::started, mCurrentWorker, &RasterClipWorker::process);
    connect(mCurrentWorker, &RasterClipWorker::progressChanged,
            this, &RasterClipServiceImpl::progressChanged);
    connect(mCurrentWorker, &RasterClipWorker::errorOccurred,
            this, &RasterClipServiceImpl::errorOccurred);
    connect(mCurrentWorker, &RasterClipWorker::finished, this,
        [this](bool success, const QString& outputPath) {
            mRunning = false;
            emit finished(success, outputPath);
            cleanupWorker();
        });
    connect(mCurrentThread, &QThread::finished, mCurrentWorker, &QObject::deleteLater);

    mRunning = true;
    mCurrentThread->start();
}

void RasterClipServiceImpl::cancel()
{
    if (mCurrentWorker)
        mCurrentWorker->requestCancel();
    mRunning = false;
}

bool RasterClipServiceImpl::isRunning() const
{
    return mRunning;
}

void RasterClipServiceImpl::cleanupWorker()
{
    if (mCurrentThread) {
        mCurrentThread->quit();
        if (!mCurrentThread->wait(5000))
            mCurrentThread->terminate();
        mCurrentThread = nullptr;
        mCurrentWorker = nullptr;
    }
}
