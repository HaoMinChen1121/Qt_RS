#include "VectorReprojectionServiceImpl.h"
#include "controllers/WorkerManager.h"
#include "controllers/workers/VectorReprojectionWorker.h"
#include <QThread>

VectorReprojectionServiceImpl::VectorReprojectionServiceImpl(WorkerManager* wm, QObject* parent)
    : IVectorReprojectionService(), mWm(wm) { setParent(parent); }

void VectorReprojectionServiceImpl::execute(const VectorReprojectionParams& params)
{
    if (mRunning && mWorker) { mWorker->requestCancel(); cleanupWorker(); }
    mWorker = new VectorReprojectionWorker(params);
    mThread = mWm->allocateThread("VectorReproj");
    mWorker->moveToThread(mThread);
    connect(mThread, &QThread::started, mWorker, &VectorReprojectionWorker::process);
    connect(mWorker, &VectorReprojectionWorker::progressChanged, this, &IVectorReprojectionService::progressChanged);
    connect(mWorker, &VectorReprojectionWorker::errorOccurred, this, &IVectorReprojectionService::errorOccurred);
    connect(mWorker, &VectorReprojectionWorker::finished, this, [this](bool ok, const QString& path) { mRunning = false; emit finished(ok, path); cleanupWorker(); });
    connect(mThread, &QThread::finished, mWorker, &QObject::deleteLater);
    mRunning = true; mThread->start();
}

void VectorReprojectionServiceImpl::cancel() { if (mWorker) mWorker->requestCancel(); mRunning = false; }
bool VectorReprojectionServiceImpl::isRunning() const { return mRunning; }
void VectorReprojectionServiceImpl::cleanupWorker() { if (mThread) { mThread->quit(); mThread->wait(5000); mThread = nullptr; mWorker = nullptr; } }
