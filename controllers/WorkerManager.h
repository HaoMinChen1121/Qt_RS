#ifndef WORKERMANAGER_H
#define WORKERMANAGER_H

#include <QObject>
#include <QThread>
#include <QList>

/**
 * @brief 管理 QThread 池，负责线程分配和清理
 */
class WorkerManager : public QObject
{
    Q_OBJECT
public:
    explicit WorkerManager(QObject* parent = nullptr);
    ~WorkerManager();

    QThread* allocateThread(const QString& threadName);
    void shutdownAll();

private:
    QList<QThread*> mThreads;
};

#endif // WORKERMANAGER_H
