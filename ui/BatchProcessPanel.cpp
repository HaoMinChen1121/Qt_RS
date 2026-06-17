#include "BatchProcessPanel.h"

#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QSplitter>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QDateTime>

BatchProcessPanel::BatchProcessPanel(QWidget* parent)
    : QWidget(parent)
    {
    setupUI();
}

void BatchProcessPanel::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // 任务队列表格
    auto* taskGroup = new QGroupBox(tr("任务队列"), this);
    auto* taskLayout = new QVBoxLayout(taskGroup);

    mTaskTable = new QTableWidget(0, 5, taskGroup);
    mTaskTable->setHorizontalHeaderLabels({
        tr("任务名称"), tr("类型"), tr("状态"), tr("进度"), tr("耗时")
    });
    mTaskTable->horizontalHeader()->setStretchLastSection(true);
    mTaskTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    mTaskTable->setAlternatingRowColors(true);
    mTaskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTaskTable->setMaximumHeight(200);
    taskLayout->addWidget(mTaskTable);
    mainLayout->addWidget(taskGroup);

    // 总进度
    mOverallProgress = new QProgressBar(this);
    mOverallProgress->setTextVisible(true);
    mainLayout->addWidget(mOverallProgress);

    // 控制按钮
    auto* controlLayout = new QHBoxLayout();
    mStartBtn = new QPushButton(tr("开始全部"), this);
    mStartBtn->setStyleSheet("QPushButton { font-weight: bold; }");
    mPauseBtn = new QPushButton(tr("暂停"), this);
    mResumeBtn = new QPushButton(tr("继续"), this);
    mCancelBtn = new QPushButton(tr("取消"), this);
    mRetryBtn = new QPushButton(tr("重试选中"), this);

    mPauseBtn->setEnabled(false);
    mResumeBtn->setEnabled(false);
    mCancelBtn->setEnabled(false);

    controlLayout->addWidget(mStartBtn);
    controlLayout->addWidget(mPauseBtn);
    controlLayout->addWidget(mResumeBtn);
    controlLayout->addWidget(mCancelBtn);
    controlLayout->addWidget(mRetryBtn);
    controlLayout->addStretch();
    mainLayout->addLayout(controlLayout);

    // 日志查看器
    auto* logGroup = new QGroupBox(tr("处理日志"), this);
    auto* logLayout = new QVBoxLayout(logGroup);
    mLogViewer = new QTextEdit(logGroup);
    mLogViewer->setReadOnly(true);
    mLogViewer->setMaximumHeight(150);
    logLayout->addWidget(mLogViewer);

    auto* logBtnLayout = new QHBoxLayout();
    mClearLogBtn = new QPushButton(tr("清空日志"), logGroup);
    mExportReportBtn = new QPushButton(tr("导出报告..."), logGroup);
    logBtnLayout->addWidget(mClearLogBtn);
    logBtnLayout->addWidget(mExportReportBtn);
    logBtnLayout->addStretch();
    logLayout->addLayout(logBtnLayout);
    mainLayout->addWidget(logGroup);

    // 状态
    mStatusLabel = new QLabel(tr("就绪 — 添加任务后点击\"开始全部\"启动批处理"), this);
    mStatusLabel->setAlignment(Qt::AlignCenter);
    mStatusLabel->setStyleSheet("color: gray;");
    mainLayout->addWidget(mStatusLabel);

    // 信号连接
    connect(mStartBtn, &QPushButton::clicked, this, &BatchProcessPanel::onStartAll);
    connect(mPauseBtn, &QPushButton::clicked, this, &BatchProcessPanel::onPause);
    connect(mResumeBtn, &QPushButton::clicked, this, &BatchProcessPanel::onResume);
    connect(mCancelBtn, &QPushButton::clicked, this, &BatchProcessPanel::onCancel);
    connect(mRetryBtn, &QPushButton::clicked, this, &BatchProcessPanel::onRetrySelected);
    connect(mExportReportBtn, &QPushButton::clicked, this, &BatchProcessPanel::onExportReport);
    connect(mClearLogBtn, &QPushButton::clicked, this, &BatchProcessPanel::onClearLog);
}

void BatchProcessPanel::addTask(const QString& taskName, const QString& taskType)
{
    int row = mTaskTable->rowCount();
    mTaskTable->insertRow(row);

    mTaskTable->setItem(row, 0, new QTableWidgetItem(taskName));
    mTaskTable->setItem(row, 1, new QTableWidgetItem(taskType));
    mTaskTable->setItem(row, 2, new QTableWidgetItem(tr("待处理")));
    mTaskTable->setItem(row, 3, new QTableWidgetItem("0%"));
    mTaskTable->setItem(row, 4, new QTableWidgetItem("--"));

    ++mTaskIdCounter;
}

void BatchProcessPanel::removeTask(int taskId)
{
    Q_UNUSED(taskId);
    QList<QTableWidgetItem*> selected = mTaskTable->selectedItems();
    QSet<int> rows;
    for (auto* item : selected) rows.insert(item->row());
    QList<int> sorted = rows.values();
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int row : sorted) mTaskTable->removeRow(row);
}

void BatchProcessPanel::clearAllTasks()
{
    mTaskTable->setRowCount(0);
}

void BatchProcessPanel::onStartAll()
{
    if (mTaskTable->rowCount() == 0)
    {
        mStatusLabel->setText(tr("任务队列为空"));
        mStatusLabel->setStyleSheet("color: red;");
        return;
    }

    mStartBtn->setEnabled(false);
    mPauseBtn->setEnabled(true);
    mCancelBtn->setEnabled(true);
    mResumeBtn->setEnabled(false);

    mStatusLabel->setText(tr("批处理运行中..."));
    mStatusLabel->setStyleSheet("color: orange;");

    emit startAllRequested();
}

void BatchProcessPanel::onPause()
{
    mPauseBtn->setEnabled(false);
    mResumeBtn->setEnabled(true);
    mStatusLabel->setText(tr("批处理已暂停"));
    mStatusLabel->setStyleSheet("color: orange;");
    emit pauseRequested();
}

void BatchProcessPanel::onResume()
{
    mPauseBtn->setEnabled(true);
    mResumeBtn->setEnabled(false);
    mStatusLabel->setText(tr("批处理继续运行..."));
    mStatusLabel->setStyleSheet("color: orange;");
    emit resumeRequested();
}

void BatchProcessPanel::onCancel()
{
    mStartBtn->setEnabled(true);
    mPauseBtn->setEnabled(false);
    mResumeBtn->setEnabled(false);
    mCancelBtn->setEnabled(false);
    mStatusLabel->setText(tr("批处理已取消"));
    mStatusLabel->setStyleSheet("color: gray;");
    emit cancelRequested();
}

void BatchProcessPanel::onRetrySelected()
{
    QList<QTableWidgetItem*> selected = mTaskTable->selectedItems();
    if (selected.isEmpty())
    {
        mStatusLabel->setText(tr("请先在表格中选择要重试的任务"));
        mStatusLabel->setStyleSheet("color: red;");
        return;
    }
    int taskId = selected.first()->row();
    emit retryRequested(taskId);
}

void BatchProcessPanel::onExportReport()
{
    QString file = QFileDialog::getSaveFileName(this, tr("导出处理报告"),
        QString(), tr("HTML报告 (*.html);;文本文件 (*.txt);;JSON (*.json)"));
    if (!file.isEmpty())
    {
        emit reportExportRequested(file);
    }
}

void BatchProcessPanel::onClearLog()
{
    mLogViewer->clear();
}

void BatchProcessPanel::onTaskStatusChanged(int taskId, int status, int progress, double elapsedSeconds)
{
    if (taskId < 0 || taskId >= mTaskTable->rowCount()) return;

    TaskStatus s = static_cast<TaskStatus>(status);
    mTaskTable->item(taskId, 2)->setText(statusText(s));
    mTaskTable->item(taskId, 3)->setText(QString("%1%").arg(progress));
    mTaskTable->item(taskId, 4)->setText(QString("%1 秒").arg(elapsedSeconds, 0, 'f', 1));

    // 颜色标记
    QColor color;
    switch (s)
    {
    case Running: color = QColor(255, 200, 100); break;
    case Completed: color = QColor(150, 255, 150); break;
    case Failed: color = QColor(255, 150, 150); break;
    case Paused: color = QColor(200, 200, 200); break;
    default: color = Qt::white; break;
    }
    for (int col = 0; col < mTaskTable->columnCount(); ++col)
    {
        if (mTaskTable->item(taskId, col))
            mTaskTable->item(taskId, col)->setBackground(color);
    }

    // 更新总体进度
    int totalProgress = 0;
    for (int r = 0; r < mTaskTable->rowCount(); ++r)
    {
        QString progText = mTaskTable->item(r, 3)->text();
        progText.remove('%');
        totalProgress += progText.toInt();
    }
    if (mTaskTable->rowCount() > 0)
    {
        mOverallProgress->setValue(totalProgress / mTaskTable->rowCount());
    }
}

void BatchProcessPanel::onLogMessage(const QString& taskName, const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    mLogViewer->append(QString("[%1] [%2] %3").arg(timestamp, taskName, message));
}

void BatchProcessPanel::onReportReady(const QString& reportContent)
{
    mLogViewer->append("\n===== 处理报告 =====\n");
    mLogViewer->append(reportContent);
    mLogViewer->append("===== 报告结束 =====\n");
    mStatusLabel->setText(tr("处理报告已生成"));
    mStatusLabel->setStyleSheet("color: green;");
}

void BatchProcessPanel::updateTaskStatus(int taskId, TaskStatus status, int progress)
{
    onTaskStatusChanged(taskId, static_cast<int>(status), progress, 0);
}

QString BatchProcessPanel::statusText(TaskStatus status) const
{
    switch (status)
    {
    case Pending: return tr("待处理");
    case Running: return tr("运行中");
    case Paused: return tr("已暂停");
    case Completed: return tr("已完成");
    case Failed: return tr("失败");
    case Cancelled: return tr("已取消");
    default: return tr("未知");
    }
}
