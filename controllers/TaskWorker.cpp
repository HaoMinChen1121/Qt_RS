#include "TaskWorker.h"

TaskWorker::TaskWorker(QObject* parent)
    : QObject(parent)
    , m_cancelled(0)
    {
}

void TaskWorker::requestCancel()
{
    m_cancelled.storeRelaxed(1);
}

bool TaskWorker::isCancelled() const
{
    return m_cancelled.loadRelaxed() == 1;
}
