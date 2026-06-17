#ifndef BATCHPROCESSPANEL_H
#define BATCHPROCESSPANEL_H

#include <QWidget>

class QTableWidget;
class QPushButton;
class QLabel;
class QProgressBar;
class QTextEdit;
class QSplitter;

/**
 * @brief 批处理引擎面板（纯表示层，嵌入Ribbon，可选模块）
 * @details 提供任务队列管理、控制操作（暂停/继续/重试）、进度监控
 *          和处理报告查看的图形界面。
 */
class BatchProcessPanel : public QWidget
{
    Q_OBJECT

public:
    explicit BatchProcessPanel(QWidget* parent = nullptr);

    /** 任务状态枚举 */
    enum TaskStatus { Pending, Running, Paused, Completed, Failed, Cancelled };

    /** 添加任务到队列 */
    void addTask(const QString& taskName, const QString& taskType);
    /** 移除任务 */
    void removeTask(int taskId);
    /** 清空任务队列 */
    void clearAllTasks();

signals:
    /** 请求开始所有待处理任务 */
    void startAllRequested();
    /** 请求暂停当前任务 */
    void pauseRequested();
    /** 请求继续暂停的任务 */
    void resumeRequested();
    /** 请求取消当前任务 */
    void cancelRequested();
    /** 请求重试失败的任务 */
    void retryRequested(int taskId);
    /** 请求导出处理报告 */
    void reportExportRequested(const QString& filePath);

public slots:
    /** 更新任务状态 */
    void onTaskStatusChanged(int taskId, int status, int progress, double elapsedSeconds);
    /** 添加日志消息 */
    void onLogMessage(const QString& taskName, const QString& message);
    /** 处理报告就绪 */
    void onReportReady(const QString& reportContent);

private slots:
    void onStartAll();
    void onPause();
    void onResume();
    void onCancel();
    void onRetrySelected();
    void onExportReport();
    void onClearLog();

private:
    void setupUI();
    void updateTaskStatus(int taskId, TaskStatus status, int progress = 0);
    QString statusText(TaskStatus status) const;

    QTableWidget* mTaskTable;
    QTextEdit* mLogViewer;
    QPushButton* mStartBtn;
    QPushButton* mPauseBtn;
    QPushButton* mResumeBtn;
    QPushButton* mCancelBtn;
    QPushButton* mRetryBtn;
    QPushButton* mExportReportBtn;
    QPushButton* mClearLogBtn;
    QProgressBar* mOverallProgress;
    QLabel* mStatusLabel;

    int mTaskIdCounter = 0;
};

#endif // BATCHPROCESSPANEL_H
