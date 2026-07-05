#include "LegendPanel.h"

#include <QTreeWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QIcon>
#include <QPixmap>
#include <QPainter>

#include <qgsvectorlayer.h>
#include <qgsrenderer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgsgraduatedsymbolrenderer.h>
#include <qgssymbol.h>

static QIcon colorSwatchIcon(const QColor& color)
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::darkGray);
    p.setBrush(color);
    p.drawRoundedRect(1, 1, 14, 14, 2, 2);
    p.end();
    return QIcon(pm);
}

LegendPanel::LegendPanel(QWidget* parent)
    : QDockWidget(QStringLiteral("图例"), parent)
{
    setObjectName(QStringLiteral("LegendPanel"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setMinimumWidth(160);

    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    mLegendTree = new QTreeWidget(container);
    mLegendTree->setHeaderLabels({QStringLiteral("符号"), QStringLiteral("分类")});
    mLegendTree->setColumnCount(2);
    mLegendTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    mLegendTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    mLegendTree->setRootIsDecorated(false);
    mLegendTree->setIconSize(QSize(16, 16));
    mLegendTree->setAlternatingRowColors(true);

    layout->addWidget(mLegendTree);
    setWidget(container);
}

void LegendPanel::showLegendForLayer(QgsMapLayer* layer)
{
    mLegendTree->clear();
    auto* vl = qobject_cast<QgsVectorLayer*>(layer);
    if (!vl || !vl->renderer())
        return;

    QgsFeatureRenderer* renderer = vl->renderer();

    if (auto* single = dynamic_cast<QgsSingleSymbolRenderer*>(renderer))
    {
        auto* item = new QTreeWidgetItem();
        item->setIcon(0, colorSwatchIcon(single->symbol()->color()));
        item->setText(1, vl->name());
        mLegendTree->addTopLevelItem(item);
    }
    else if (auto* cat = dynamic_cast<QgsCategorizedSymbolRenderer*>(renderer))
    {
        for (int i = 0; i < cat->categories().size(); ++i)
        {
            const QgsRendererCategory& c = cat->categories()[i];
            auto* item = new QTreeWidgetItem();
            item->setIcon(0, colorSwatchIcon(c.symbol()->color()));
            item->setText(1, c.label());
            mLegendTree->addTopLevelItem(item);
        }
    }
    else if (auto* grad = dynamic_cast<QgsGraduatedSymbolRenderer*>(renderer))
    {
        for (int i = 0; i < grad->ranges().size(); ++i)
        {
            const QgsRendererRange& r = grad->ranges()[i];
            auto* item = new QTreeWidgetItem();
            item->setIcon(0, colorSwatchIcon(r.symbol()->color()));
            item->setText(1, r.label());
            mLegendTree->addTopLevelItem(item);
        }
    }
}

void LegendPanel::clearLegend()
{
    mLegendTree->clear();
}
