#include "DefineProjectionServiceImpl.h"
#include "controllers/WorkerManager.h"
#include "controllers/workers/DefineProjectionWorker.h"
#include <QThread>

DefineProjectionServiceImpl::DefineProjectionServiceImpl(WorkerManager* wm, QObject* parent)
    : IDefineProjectionService(), mWm(wm) { setParent(parent); }

void DefineProjectionServiceImpl::execute(const DefineProjectionParams& params)
{
    if (mRunning && mWorker) { mWorker->requestCancel(); cleanupWorker(); }
    mWorker = new DefineProjectionWorker(params);
    mThread = mWm->allocateThread("DefineProj");
    mWorker->moveToThread(mThread);
    connect(mThread, &QThread::started, mWorker, &DefineProjectionWorker::process);
    connect(mWorker, &DefineProjectionWorker::progressChanged, this, &IDefineProjectionService::progressChanged);
    connect(mWorker, &DefineProjectionWorker::errorOccurred, this, &IDefineProjectionService::errorOccurred);
    connect(mWorker, &DefineProjectionWorker::finished, this, [this](bool ok, const QString& path) { mRunning = false; emit finished(ok, path); cleanupWorker(); });
    connect(mThread, &QThread::finished, mWorker, &QObject::deleteLater);
    mRunning = true; mThread->start();
}

void DefineProjectionServiceImpl::cancel() { if (mWorker) mWorker->requestCancel(); mRunning = false; }
bool DefineProjectionServiceImpl::isRunning() const { return mRunning; }
void DefineProjectionServiceImpl::cleanupWorker() { if (mThread) { mThread->quit(); mThread->wait(5000); mThread = nullptr; mWorker = nullptr; } }
