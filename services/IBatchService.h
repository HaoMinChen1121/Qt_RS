#ifndef IBATCHSERVICE_H
#define IBATCHSERVICE_H

#include "services/IProcessingService.h"
#include "domain/ProcessingTask.h"

class IBatchService : public IProcessingService
{
    Q_OBJECT
public:
    virtual void addTasks(const QList<ProcessingTask>& tasks) = 0;
    virtual void startAll() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void cancel() override = 0;
    virtual void retry(int taskId) = 0;
    virtual void clearQueue() = 0;
    virtual void exportReport(const QString& filePath) = 0;

signals:
    void taskStatusChanged(int taskId, int status, int progress, double elapsedSeconds);
    void logMessage(const QString& taskName, const QString& message);
    void reportReady(const QString& reportContent);
};

#endif // IBATCHSERVICE_H
