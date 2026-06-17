#include "WorkflowPanel.h"

#include <QListWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>

WorkflowPanel::WorkflowPanel(QWidget* parent)
    : QWidget(parent)
    {
    setupUI();
}

void WorkflowPanel::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // 中间区域使用分割器：左侧工具箱 + 中间画布 + 右侧属性/预览
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // === 左侧：节点工具箱 ===
    auto* toolboxContainer = new QWidget(splitter);
    auto* toolboxLayout = new QVBoxLayout(toolboxContainer);
    toolboxLayout->setContentsMargins(0, 0, 0, 0);

    auto* toolboxGroup = new QGroupBox(tr("处理节点"), toolboxContainer);
    auto* toolboxGroupLayout = new QVBoxLayout(toolboxGroup);
    mNodeToolbox = new QListWidget(toolboxGroup);
    mNodeToolbox->setDragDropMode(QAbstractItemView::DragOnly);
    mNodeToolbox->setMaximumWidth(140);

    // 添加处理节点
    mNodeToolbox->addItem(tr("读取影像"));
    mNodeToolbox->addItem(tr("辐射定标"));
    mNodeToolbox->addItem(tr("大气校正"));
    mNodeToolbox->addItem(tr("几何校正"));
    mNodeToolbox->addItem(tr("图像裁剪"));
    mNodeToolbox->addItem(tr("重采样"));
    mNodeToolbox->addItem(tr("图像融合"));
    mNodeToolbox->addItem(tr("影像镶嵌"));
    mNodeToolbox->addItem(tr("输出影像"));

    toolboxGroupLayout->addWidget(mNodeToolbox);
    toolboxLayout->addWidget(toolboxGroup);
    splitter->addWidget(toolboxContainer);

    // === 中间：流程画布 ===
    auto* canvasContainer = new QWidget(splitter);
    auto* canvasLayout = new QVBoxLayout(canvasContainer);
    canvasLayout->setContentsMargins(0, 0, 0, 0);

    auto* canvasGroup = new QGroupBox(tr("处理流程"), canvasContainer);
    auto* canvasGroupLayout = new QVBoxLayout(canvasGroup);
    mCanvasScene = new QGraphicsScene(canvasGroup);
    mCanvasScene->setSceneRect(0, 0, 400, 300);
    mCanvasView = new QGraphicsView(mCanvasScene, canvasGroup);
    mCanvasView->setAcceptDrops(true);
    mCanvasView->setRenderHint(QPainter::Antialiasing);
    mCanvasView->setMinimumWidth(200);
    canvasGroupLayout->addWidget(mCanvasView);
    canvasLayout->addWidget(canvasGroup);
    splitter->addWidget(canvasContainer);

    // === 右侧：属性 + 预览 ===
    auto* rightContainer = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto* propGroup = new QGroupBox(tr("节点属性"), rightContainer);
    auto* propLayout = new QVBoxLayout(propGroup);
    mPropertyEditor = new QTextEdit(propGroup);
    mPropertyEditor->setMaximumHeight(150);
    mPropertyEditor->setPlaceholderText(tr("选择节点以编辑属性..."));
    propLayout->addWidget(mPropertyEditor);
    rightLayout->addWidget(propGroup);

    auto* previewGroup = new QGroupBox(tr("实时预览"), rightContainer);
    auto* previewLayout = new QVBoxLayout(previewGroup);
    mPreviewLabel = new QLabel(tr("预览窗口 (缩略图)"), previewGroup);
    mPreviewLabel->setAlignment(Qt::AlignCenter);
    mPreviewLabel->setMinimumHeight(120);
    mPreviewLabel->setStyleSheet("QLabel { background-color: #f0f0f0; border: 1px dashed #ccc; }");
    previewLayout->addWidget(mPreviewLabel);
    rightLayout->addWidget(previewGroup);
    splitter->addWidget(rightContainer);

    splitter->setSizes({140, 300, 160});
    mainLayout->addWidget(splitter);

    // 底部操作栏
    auto* bottomLayout = new QHBoxLayout();

    mRunBtn = new QPushButton(tr("运行工作流"), this);
    mRunBtn->setStyleSheet("QPushButton { font-weight: bold; min-height: 28px; }");
    mSaveBtn = new QPushButton(tr("保存模板..."), this);
    mLoadBtn = new QPushButton(tr("加载模板..."), this);
    mClearBtn = new QPushButton(tr("清空"), this);

    bottomLayout->addWidget(mRunBtn);
    bottomLayout->addWidget(mSaveBtn);
    bottomLayout->addWidget(mLoadBtn);
    bottomLayout->addWidget(mClearBtn);
    bottomLayout->addStretch();
    mainLayout->addLayout(bottomLayout);

    // 进度条
    mProgressBar = new QProgressBar(this);
    mProgressBar->setVisible(false);
    mainLayout->addWidget(mProgressBar);

    // 状态标签
    mStatusLabel = new QLabel(tr("就绪 — 拖拽节点到画布开始构建处理流程"), this);
    mStatusLabel->setAlignment(Qt::AlignCenter);
    mStatusLabel->setStyleSheet("color: gray;");
    mainLayout->addWidget(mStatusLabel);

    connect(mRunBtn, &QPushButton::clicked, this, &WorkflowPanel::onRunWorkflow);
    connect(mSaveBtn, &QPushButton::clicked, this, &WorkflowPanel::onSaveTemplate);
    connect(mLoadBtn, &QPushButton::clicked, this, &WorkflowPanel::onLoadTemplate);
    connect(mClearBtn, &QPushButton::clicked, this, &WorkflowPanel::onClearWorkflow);
    connect(mNodeToolbox, &QListWidget::itemDoubleClicked, this, &WorkflowPanel::onNodeDoubleClicked);
}

void WorkflowPanel::onSaveTemplate()
{
    QString file = QFileDialog::getSaveFileName(this, tr("保存工作流模板"),
        QString(), tr("工作流模板 (*.wft *.xml *.json);;所有文件 (*.*)"));
    if (!file.isEmpty())
    {
        emit workflowSaveRequested(file);
    }
}

void WorkflowPanel::onLoadTemplate()
{
    QString file = QFileDialog::getOpenFileName(this, tr("加载工作流模板"),
        QString(), tr("工作流模板 (*.wft *.xml *.json);;所有文件 (*.*)"));
    if (!file.isEmpty())
    {
        emit workflowLoadRequested(file);
    }
}

void WorkflowPanel::onRunWorkflow()
{
    if (mNodeSequence.isEmpty())
    {
        mStatusLabel->setText(tr("工作流为空，请先添加处理节点"));
        mStatusLabel->setStyleSheet("color: red;");
        return;
    }

    mProgressBar->setVisible(true);
    mProgressBar->setValue(0);
    mStatusLabel->setText(tr("工作流运行中..."));
    mStatusLabel->setStyleSheet("color: orange;");

    emit workflowRunRequested(mNodeSequence);
}

void WorkflowPanel::onClearWorkflow()
{
    mCanvasScene->clear();
    mNodeSequence.clear();
    mPropertyEditor->clear();
    mStatusLabel->setText(tr("画布已清空"));
    mStatusLabel->setStyleSheet("color: gray;");
}

void WorkflowPanel::onNodeDoubleClicked(QListWidgetItem* item)
{
    // TODO: 在画布上添加节点表示
    mNodeSequence.append(item->text());
    mPropertyEditor->append(tr("[添加节点] %1").arg(item->text()));
    mStatusLabel->setText(tr("已添加节点: %1 (共 %2 个节点)")
        .arg(item->text())
        .arg(mNodeSequence.size()));
}

void WorkflowPanel::onWorkflowProgress(int percent, const QString& nodeName, const QString& statusMsg)
{
    mProgressBar->setValue(percent);
    mStatusLabel->setText(tr("[%1] %2").arg(nodeName, statusMsg));
    mStatusLabel->setStyleSheet("color: orange;");
}

void WorkflowPanel::onWorkflowFinished(bool success, const QString& outputPath)
{
    mProgressBar->setVisible(false);
    if (success)
    {
        mStatusLabel->setText(tr("工作流完成: %1").arg(outputPath));
        mStatusLabel->setStyleSheet("color: green;");
    } else
    {
        mStatusLabel->setText(tr("工作流执行失败"));
        mStatusLabel->setStyleSheet("color: red;");
    }
}

void WorkflowPanel::onWorkflowError(const QString& nodeName, const QString& errorMsg)
{
    mStatusLabel->setText(tr("节点 [%1] 错误: %2").arg(nodeName, errorMsg));
    mStatusLabel->setStyleSheet("color: red;");
}
