#ifndef BATCHSERVICEIMPL_H
#define BATCHSERVICEIMPL_H

#include "services/IBatchService.h"

class BatchServiceImpl : public IBatchService
{
    Q_OBJECT
public:
    void addTasks(const QList<ProcessingTask>& tasks) override;
    void startAll() override;
    void pause() override;
    void resume() override;
    void cancel() override;
    void retry(int taskId) override;
    void clearQueue() override;
    void exportReport(const QString& filePath) override;
    bool isRunning() const override;

private:
    QList<ProcessingTask> mQueue;
    bool mRunning = false;
};

#endif // BATCHSERVICEIMPL_H
