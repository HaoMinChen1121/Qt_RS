#include "RasterReprojectionServiceImpl.h"
#include "controllers/WorkerManager.h"
#include "controllers/workers/RasterReprojectionWorker.h"
#include <QThread>

RasterReprojectionServiceImpl::RasterReprojectionServiceImpl(WorkerManager* wm, QObject* parent)
    : IRasterReprojectionService(), mWm(wm) { setParent(parent); }

void RasterReprojectionServiceImpl::execute(const RasterReprojectionParams& params)
{
    if (mRunning && mWorker) { mWorker->requestCancel(); cleanupWorker(); }
    mWorker = new RasterReprojectionWorker(params);
    mThread = mWm->allocateThread("RasterReproj");
    mWorker->moveToThread(mThread);
    connect(mThread, &QThread::started, mWorker, &RasterReprojectionWorker::process);
    connect(mWorker, &RasterReprojectionWorker::progressChanged, this, &IRasterReprojectionService::progressChanged);
    connect(mWorker, &RasterReprojectionWorker::errorOccurred, this, &IRasterReprojectionService::errorOccurred);
    connect(mWorker, &RasterReprojectionWorker::finished, this, [this](bool ok, const QString& path) { mRunning = false; emit finished(ok, path); cleanupWorker(); });
    connect(mThread, &QThread::finished, mWorker, &QObject::deleteLater);
    mRunning = true; mThread->start();
}

void RasterReprojectionServiceImpl::cancel() { if (mWorker) mWorker->requestCancel(); mRunning = false; }
bool RasterReprojectionServiceImpl::isRunning() const { return mRunning; }
void RasterReprojectionServiceImpl::cleanupWorker() { if (mThread) { mThread->quit(); mThread->wait(5000); mThread = nullptr; mWorker = nullptr; } }
