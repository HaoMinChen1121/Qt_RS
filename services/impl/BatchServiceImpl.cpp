#include "BatchServiceImpl.h"
#include <QDebug>

void BatchServiceImpl::addTasks(const QList<ProcessingTask>& tasks)
{
    qDebug() << "[BatchService] addTasks:" << tasks.size();
    mQueue.append(tasks);
    for (const auto& t : tasks)
        emit logMessage(t.taskName, QStringLiteral("Task added to queue"));
}

void BatchServiceImpl::startAll()
{
    qDebug() << "[BatchService] startAll: queueSize=" << mQueue.size();
    mRunning = true;
    for (int i = 0; i < mQueue.size(); ++i)
    {
        mQueue[i].status = 1; // Running
        mQueue[i].progress = (i + 1) * 100 / mQueue.size();
        mQueue[i].elapsedSeconds = 1.0;
        emit taskStatusChanged(mQueue[i].taskId, mQueue[i].status,
                               mQueue[i].progress, mQueue[i].elapsedSeconds);
        emit logMessage(mQueue[i].taskName, QStringLiteral("Processing..."));
    }
    for (int i = 0; i < mQueue.size(); ++i)
    {
        mQueue[i].status = 3; // Completed
        mQueue[i].progress = 100;
        emit taskStatusChanged(mQueue[i].taskId, mQueue[i].status,
                               mQueue[i].progress, mQueue[i].elapsedSeconds);
    }
    mRunning = false;
    emit finished(true, QString());
}

void BatchServiceImpl::pause()
{
    qDebug() << "[BatchService] pause";
}

void BatchServiceImpl::resume()
{
    qDebug() << "[BatchService] resume";
}

void BatchServiceImpl::cancel()
{
    qDebug() << "[BatchService] cancel";
    mRunning = false;
}

void BatchServiceImpl::retry(int taskId)
{
    qDebug() << "[BatchService] retry:" << taskId;
}

void BatchServiceImpl::clearQueue()
{
    qDebug() << "[BatchService] clearQueue";
    mQueue.clear();
}

void BatchServiceImpl::exportReport(const QString& filePath)
{
    qDebug() << "[BatchService] exportReport:" << filePath;
    emit reportReady(QStringLiteral("Report content placeholder"));
}

bool BatchServiceImpl::isRunning() const
{
    return mRunning;
}
