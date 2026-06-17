#ifndef IPROCESSINGSERVICE_H
#define IPROCESSINGSERVICE_H

#include <QObject>
#include <QString>

/**
 * @brief 所有处理服务的异步基础接口
 *
 * 实现类在子线程执行工作，通过信号报告进度和结果。
 * ApplicationController 将 UI 信号连接到这些方法，并将这些信号连接到 UI 槽。
 */
class IProcessingService : public QObject
{
    Q_OBJECT
public:
    virtual ~IProcessingService() = default;

    virtual void cancel() = 0;
    virtual bool isRunning() const = 0;

signals:
    void progressChanged(int percent, const QString& statusMessage);
    void finished(bool success, const QString& outputPath);
    void errorOccurred(const QString& errorMessage);
};

#endif // IPROCESSINGSERVICE_H
