#ifndef TASKWORKER_H
#define TASKWORKER_H

#include <QObject>
#include <QAtomicInt>

/**
 * @brief 异步工作单元基类
 *
 * Service 实现创建具体子类，moveToThread() 后在子线程调用 process()。
 * process() 中检查 isCancelled() 以支持中途取消。
 */
class TaskWorker : public QObject
{
    Q_OBJECT
public:
    explicit TaskWorker(QObject* parent = nullptr);

    void requestCancel();

protected:
    bool isCancelled() const;

signals:
    void progressChanged(int percent, const QString& statusMessage);
    void finished(bool success, const QString& outputPath);
    void errorOccurred(const QString& errorMessage);

public slots:
    virtual void process() = 0;

private:
    QAtomicInt m_cancelled;
};

#endif // TASKWORKER_H
