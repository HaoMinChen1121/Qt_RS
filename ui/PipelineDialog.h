#ifndef PIPELINEDIALOG_H
#define PIPELINEDIALOG_H

#include <QDialog>
#include "domain/Project.h"

class QListWidget;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QLabel;
class QProgressBar;
class QTextEdit;

class PipelineDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PipelineDialog(QWidget* parent = nullptr);

    void setProject(const Project& project);
    Project project() const;

signals:
    void runRequested(const Project& project);
    void saveRequested(const QString& filePath);
    void loadRequested(const QString& filePath);

public slots:
    void onStageProgress(const QString& stageId, int percent, const QString& status);
    void onPipelineFinished(bool success, const QString& outputPath);
    void onPipelineError(const QString& nodeId, const QString& error);

private slots:
    void onAddFiles();
    void onRemoveFile();
    void onFileSelectionChanged();
    void onRoleChanged(int index);
    void onSaveProject();
    void onLoadProject();
    void onRun();

private:
    void setupUI();
    void populateStages(const PipelineDefinition& pipeline);
    void updateFileInfo(int row);
    void syncFromUI();
    void updateBandFieldStates();

    // 文件列表
    QListWidget* mFileList;
    QPushButton* mAddBtn;
    QPushButton* mRemoveBtn;

    // 当前选中文件的属性
    QComboBox*   mRoleCombo;
    QLabel*      mSensorLabel;
    QLineEdit*   mBandR;
    QLineEdit*   mBandG;
    QLineEdit*   mBandB;
    QLineEdit*   mBandPan;

    // 处理阶段
    struct StageWidget {
        QCheckBox* checkbox;
        QPushButton* configBtn;
        PipelineStage stage;
    };
    QList<StageWidget> mStageWidgets;
    void onStageConfig(int idx);

    // 输出
    QLineEdit*   mOutputDir;
    QPushButton* mOutputBrowseBtn;
    QCheckBox*   mCleanupCheck;
    QCheckBox*   mAutoConfirmCheck;

    // 工程名称
    QLineEdit*   mProjectName;

    // 操作
    QPushButton* mRunBtn;
    QPushButton* mSaveBtn;
    QPushButton* mLoadBtn;
    QProgressBar* mProgressBar;
    QLabel*      mStatusLabel;

    // 工程数据
    Project mProject;
};

#endif // PIPELINEDIALOG_H
