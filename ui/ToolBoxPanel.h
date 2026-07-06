#ifndef TOOLBOXPANEL_H
#define TOOLBOXPANEL_H

#include <QDockWidget>
#include "domain/ToolDefinition.h"

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QSplitter;
class QTextBrowser;

class ToolBoxPanel : public QDockWidget
{
    Q_OBJECT
public:
    explicit ToolBoxPanel(QWidget* parent = nullptr);

    void registerTool(const ToolDefinition& tool);
    void registerTools(const QList<ToolDefinition>& tools);
    void appendResult(bool success, const QString& toolName, const QString& message);

signals:
    void toolRequested(const QString& toolId);

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onSearchTextChanged(const QString& text);

private:
    void setupUI();
    QTreeWidgetItem* ensureCategory(const QString& category);

    QLineEdit* mSearchEdit;
    QTreeWidget* mTree;
    QTextBrowser* mResultBrowser;
    QSplitter* mSplitter;

    QList<ToolDefinition> mTools;
    QStringList mCategories;
};

#endif // TOOLBOXPANEL_H
