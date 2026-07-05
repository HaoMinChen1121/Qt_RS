#ifndef LEGENDPANEL_H
#define LEGENDPANEL_H

#include <QDockWidget>

class QTreeWidget;

class LegendPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit LegendPanel(QWidget* parent = nullptr);

public slots:
    void showLegendForLayer(class QgsMapLayer* layer);
    void clearLegend();

private:
    QTreeWidget* mLegendTree;
};

#endif // LEGENDPANEL_H
