#include "ToolBoxPanel.h"

#include <QVBoxLayout>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSplitter>
#include <QTextBrowser>
#include <QHeaderView>
#include <QDateTime>
#include <QApplication>

ToolBoxPanel::ToolBoxPanel(QWidget* parent)
    : QDockWidget(parent)
{
    setObjectName(QStringLiteral("ToolBoxDock"));
    setWindowTitle(QString::fromUtf8("\xe5\xa4\x84\xe7\x90\x86\xe5\xb7\xa5\xe5\x85\xb7\xe7\xae\xb1")); // 处理工具箱
    setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
                QDockWidget::DockWidgetClosable);
    setMinimumWidth(200);
    setupUI();
}

void ToolBoxPanel::setupUI()
{
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // ── Search bar ──
    mSearchEdit = new QLineEdit(container);
    mSearchEdit->setPlaceholderText(QString::fromUtf8("\xe6\x90\x9c\xe7\xb4\xa2\xe5\xb7\xa5\xe5\x85\xb7..."));
    mSearchEdit->setClearButtonEnabled(true);
    layout->addWidget(mSearchEdit);

    // ── Splitter: tree + results ──
    mSplitter = new QSplitter(Qt::Vertical, container);

    mTree = new QTreeWidget(mSplitter);
    mTree->setHeaderHidden(true);
    mTree->setRootIsDecorated(true);
    mTree->setIndentation(16);
    mTree->setAnimated(true);
    mTree->header()->setStretchLastSection(true);
    mTree->setContextMenuPolicy(Qt::CustomContextMenu);

    mResultBrowser = new QTextBrowser(mSplitter);
    mResultBrowser->setOpenExternalLinks(false);
    mResultBrowser->setMaximumHeight(120);
    mResultBrowser->setPlaceholderText(
        QString::fromUtf8("\xe6\x89\xa7\xe8\xa1\x8c\xe7\xbb\x93\xe6\x9e\x9c")); // 执行结果

    mSplitter->addWidget(mTree);
    mSplitter->addWidget(mResultBrowser);
    mSplitter->setStretchFactor(0, 3);
    mSplitter->setStretchFactor(1, 1);

    layout->addWidget(mSplitter);
    setWidget(container);

    // ── Signals ──
    connect(mSearchEdit, &QLineEdit::textChanged, this, &ToolBoxPanel::onSearchTextChanged);
    connect(mTree, &QTreeWidget::itemDoubleClicked, this, &ToolBoxPanel::onItemDoubleClicked);
}

QTreeWidgetItem* ToolBoxPanel::ensureCategory(const QString& category)
{
    for (int i = 0; i < mTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = mTree->topLevelItem(i);
        if (item->text(0) == category)
            return item;
    }
    auto* catItem = new QTreeWidgetItem(mTree);
    catItem->setText(0, category);
    catItem->setFlags(catItem->flags() | Qt::ItemIsAutoTristate);
    QFont f = catItem->font(0);
    f.setBold(true);
    catItem->setFont(0, f);
    catItem->setExpanded(true);
    mCategories.append(category);
    return catItem;
}

void ToolBoxPanel::registerTool(const ToolDefinition& tool)
{
    mTools.append(tool);

    QTreeWidgetItem* cat = ensureCategory(tool.category);
    auto* leaf = new QTreeWidgetItem(cat);
    leaf->setText(0, tool.displayName);
    leaf->setToolTip(0, tool.description);
    leaf->setData(0, Qt::UserRole, tool.toolId);
    leaf->setFlags(leaf->flags() & ~Qt::ItemIsAutoTristate);
}

void ToolBoxPanel::registerTools(const QList<ToolDefinition>& tools)
{
    for (const auto& t : tools)
        registerTool(t);
}

void ToolBoxPanel::appendResult(bool success, const QString& toolName, const QString& message)
{
    QString color = success ? QStringLiteral("green") : QStringLiteral("red");
    QString symbol = success ? QString::fromUtf8("\xe2\x9c\x93") : QString::fromUtf8("\xe2\x9c\x97");
    QString time = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
    mResultBrowser->append(QStringLiteral(
        "<span style='color:%1;'><b>%2</b></span> %3 &mdash; %4  <span style='color:gray;'>%5</span>")
        .arg(color, symbol, toolName, message, time));
}

void ToolBoxPanel::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    QString toolId = item->data(0, Qt::UserRole).toString();
    if (!toolId.isEmpty())
        emit toolRequested(toolId);
}

void ToolBoxPanel::onSearchTextChanged(const QString& text)
{
    for (int i = 0; i < mTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* cat = mTree->topLevelItem(i);
        bool anyVisible = false;
        for (int j = 0; j < cat->childCount(); ++j) {
            QTreeWidgetItem* leaf = cat->child(j);
            bool match = text.isEmpty() ||
                leaf->text(0).contains(text, Qt::CaseInsensitive) ||
                leaf->toolTip(0).contains(text, Qt::CaseInsensitive);
            leaf->setHidden(!match);
            if (match) anyVisible = true;
        }
        cat->setHidden(!anyVisible);
        if (!text.isEmpty() && anyVisible)
            cat->setExpanded(true);
    }
}
