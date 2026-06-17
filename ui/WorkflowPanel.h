#ifndef WORKFLOWPANEL_H
#define WORKFLOWPANEL_H

#include <QWidget>
#include <QStringList>
#include <QListWidgetItem>

class QListWidget;
class QGraphicsView;
class QGraphicsScene;
class QPushButton;
class QLabel;
class QProgressBar;
class QSplitter;
class QTextEdit;

/**
 * @brief 可视化任务流设计器面板（纯表示层，嵌入Ribbon，可选模块）
 * @details 提供处理节点工具箱、流程画布、属性编辑、实时预览的图形界面。
 *          支持拖拽创建处理链（读取→定标→大气校正→裁剪→重采样→融合→镶嵌→输出）。
 */
class WorkflowPanel : public QWidget
{
    Q_OBJECT

public:
    explicit WorkflowPanel(QWidget* parent = nullptr);

signals:
    /** 请求运行工作流 */
    void workflowRunRequested(const QStringList& nodeSequence);
    /** 请求保存工作流模板 */
    void workflowSaveRequested(const QString& filePath);
    /** 请求加载工作流模板 */
    void workflowLoadRequested(const QString& filePath);
    /** 请求预览指定节点的处理结果 */
    void nodePreviewRequested(const QString& nodeId);

public slots:
    void onWorkflowProgress(int percent, const QString& nodeName, const QString& statusMsg);
    void onWorkflowFinished(bool success, const QString& outputPath);
    void onWorkflowError(const QString& nodeName, const QString& errorMsg);

private slots:
    void onSaveTemplate();
    void onLoadTemplate();
    void onRunWorkflow();
    void onClearWorkflow();
    void onNodeDoubleClicked(QListWidgetItem* item);

private:
    void setupUI();

    // 节点工具箱
    QListWidget* mNodeToolbox;
    // 流程画布
    QGraphicsView* mCanvasView;
    QGraphicsScene* mCanvasScene;
    // 属性面板
    QTextEdit* mPropertyEditor;
    // 预览区
    QLabel* mPreviewLabel;
    // 操作按钮
    QPushButton* mSaveBtn;
    QPushButton* mLoadBtn;
    QPushButton* mRunBtn;
    QPushButton* mClearBtn;
    // 进度
    QProgressBar* mProgressBar;
    QLabel* mStatusLabel;

    QStringList mNodeSequence;
};

#endif // WORKFLOWPANEL_H
