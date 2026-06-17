#include "WorkerManager.h"

WorkerManager::WorkerManager(QObject* parent)
    : QObject(parent)
    {
}

WorkerManager::~WorkerManager()
{
    shutdownAll();
}

QThread* WorkerManager::allocateThread(const QString& threadName)
{
    auto* thread = new QThread(this);
    thread->setObjectName(threadName);
    mThreads.append(thread);
    return thread;
}

void WorkerManager::shutdownAll()
{
    for (auto* thread : mThreads)
    {
        thread->quit();
        thread->wait(5000);
        delete thread;
    }
    mThreads.clear();
}
