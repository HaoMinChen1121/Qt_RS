#include "LayerPanel.h"
#include "ui/BandCombinationDialog.h"

#include <QTreeWidgetItem>
#include <QToolBar>
#include <QAction>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>

LayerPanel::LayerPanel(QWidget* parent)
    : QWidget(parent)
    {
    setupUI();
    createActions();
}

void LayerPanel::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    mToolBar = new QToolBar(this);
    mToolBar->setIconSize(QSize(16, 16));
    mToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    layout->addWidget(mToolBar);

    mLayerTree = new LayerTreeWidget(this);
    mLayerTree->setHeaderLabels({ tr(""), tr("图层名称"), tr("类型") });
    mLayerTree->setColumnCount(3);
    mLayerTree->header()->setStretchLastSection(false);
    mLayerTree->header()->setSectionResizeMode(0, QHeaderView::Fixed);
    mLayerTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    mLayerTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mLayerTree->setColumnWidth(0, 30);
    mLayerTree->setSelectionMode(QAbstractItemView::SingleSelection);
    mLayerTree->setContextMenuPolicy(Qt::CustomContextMenu);
    mLayerTree->setAlternatingRowColors(true);

    layout->addWidget(mLayerTree);

    // 透明度控制
    auto* opacityLayout = new QHBoxLayout();
    auto* opacityTitle = new QLabel(tr("透明度:"), this);
    mOpacityLabel = new QLabel("100%", this);
    mOpacityLabel->setFixedWidth(40);
    mOpacityLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mOpacitySlider = new QSlider(Qt::Horizontal, this);
    mOpacitySlider->setRange(0, 100);
    mOpacitySlider->setValue(100);
    mOpacitySlider->setTickPosition(QSlider::NoTicks);
    opacityLayout->addWidget(opacityTitle);
    opacityLayout->addWidget(mOpacitySlider);
    opacityLayout->addWidget(mOpacityLabel);
    layout->addLayout(opacityLayout);

    connect(mOpacitySlider, &QSlider::valueChanged, this, [this](int val)
    {
        mOpacityLabel->setText(QStringLiteral("%1%").arg(val));
        QString layerId = currentLayerId();
        if (!layerId.isEmpty())
            emit opacityChanged(layerId, val / 100.0);
    });

    connect(mLayerTree, &QTreeWidget::itemChanged, this, &LayerPanel::onItemChanged);
    connect(mLayerTree, &QTreeWidget::itemSelectionChanged, this, &LayerPanel::onItemSelectionChanged);
    connect(mLayerTree, &QTreeWidget::customContextMenuRequested, this, &LayerPanel::onContextMenu);
    mLayerTree->setOnOrderChanged([this]() { onDragOrderChanged(); });
}

void LayerPanel::createActions()
{
    mAddAction = mToolBar->addAction(QIcon(":/icon/icon/save.svg"), tr("添加栅格图层"));
    mAddVectorAction = mToolBar->addAction(QIcon(":/icon/icon/item.svg"), tr("添加矢量图层"));
    mRemoveAction = mToolBar->addAction(QIcon(":/icon/icon/delete.svg"), tr("移除图层"));
    mToolBar->addSeparator();
    mMoveUpAction = mToolBar->addAction(QIcon(":/icon/icon/undo.svg"), tr("上移"));
    mMoveDownAction = mToolBar->addAction(QIcon(":/icon/icon/redo.svg"), tr("下移"));
    mToolBar->addSeparator();
    mZoomToAction = mToolBar->addAction(QIcon(":/icon/icon/layout.svg"), tr("缩放到图层"));
    mToolBar->addSeparator();
    mBandManagerAction = mToolBar->addAction(QIcon(":/icon/icon/folder-stats.svg"), tr("波段管理器"));
    mBandManagerAction->setCheckable(true);

    connect(mAddAction, &QAction::triggered, this, &LayerPanel::onAddLayer);
    connect(mAddVectorAction, &QAction::triggered, this, &LayerPanel::onAddVectorLayer);
    connect(mRemoveAction, &QAction::triggered, this, &LayerPanel::onRemoveLayer);
    connect(mMoveUpAction, &QAction::triggered, this, &LayerPanel::onMoveLayerUp);
    connect(mMoveDownAction, &QAction::triggered, this, &LayerPanel::onMoveLayerDown);
    connect(mZoomToAction, &QAction::triggered, this, &LayerPanel::onZoomToLayer);
    connect(mBandManagerAction, &QAction::triggered, this, [this]() {
        emit bandManagerToggleRequested();
    });
}

int LayerPanel::layerCount() const
{
    return mLayerTree->topLevelItemCount();
}

QString LayerPanel::currentLayerId() const
{
    QTreeWidgetItem* item = mLayerTree->currentItem();
    return item ? item->data(0, Qt::UserRole).toString() : QString();
}

QTreeWidgetItem* LayerPanel::findItem(const QString& layerId) const
{
    for (int i = 0; i < mLayerTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = mLayerTree->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == layerId)
            return item;
    }
    return nullptr;
}

QStringList LayerPanel::orderedLayerIds() const
{
    QStringList ids;
    for (int i = 0; i < mLayerTree->topLevelItemCount(); ++i)
    {
        ids << mLayerTree->topLevelItem(i)->data(0, Qt::UserRole).toString();
    }
    return ids;
}

// ========== Slots — 业务逻辑层反馈 ==========

void LayerPanel::onLayerLoaded(const QString& layerId, const QString& name, const QString& type)
{
    if (findItem(layerId)) return;

    auto* item = new QTreeWidgetItem();
    item->setData(0, Qt::UserRole, layerId);
    item->setCheckState(0, Qt::Checked);
    item->setText(1, name);
    item->setText(2, type);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

    // 插入到树顶部：新图层 = 最顶层
    mLayerTree->insertTopLevelItem(0, item);
    mLayerTree->setCurrentItem(item);
}

void LayerPanel::onVectorLayerLoaded(const QString& layerId, const QString& name, const QString& geometryType)
{
    if (findItem(layerId)) return;

    auto* item = new QTreeWidgetItem();
    item->setData(0, Qt::UserRole, layerId);
    item->setCheckState(0, Qt::Checked);
    item->setText(1, name);
    item->setText(2, QStringLiteral("V: %1").arg(geometryType));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

    mLayerTree->insertTopLevelItem(0, item);
    mLayerTree->setCurrentItem(item);
}

void LayerPanel::onLayerRemoved(const QString& layerId)
{
    QTreeWidgetItem* item = findItem(layerId);
    if (item)
    {
        int idx = mLayerTree->indexOfTopLevelItem(item);
        delete mLayerTree->takeTopLevelItem(idx);
    }
}

void LayerPanel::onLayerError(const QString& layerId, const QString& errorMsg)
{
    QTreeWidgetItem* item = findItem(layerId);
    if (item)
    {
        item->setToolTip(1, errorMsg);
        item->setForeground(1, QColor(255, 80, 80));
    }
}

void LayerPanel::onLayerVisibilitySet(const QString& layerId, bool visible)
{
    QTreeWidgetItem* item = findItem(layerId);
    if (item)
    {
        mLayerTree->blockSignals(true);
        item->setCheckState(0, visible ? Qt::Checked : Qt::Unchecked);
        mLayerTree->blockSignals(false);
    }
}

// ========== 私有槽 — 用户交互 ==========

void LayerPanel::onAddLayer()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("选择栅格图层文件"),
        QString(),
        tr("所有栅格格式 (*.zip *.SAFE *_MTL.txt *.tif *.tiff *.img);;"
           "Sentinel-2 (*.zip *.SAFE);;"
           "Landsat (*_MTL.txt);;"
           "通用栅格 (*.tif *.tiff *.img);;"
           "所有文件 (*.*)"));
    if (!files.isEmpty())
    {
        emit layerAddRequested(files);
    }
}

void LayerPanel::onAddVectorLayer()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("选择矢量图层文件"),
        QString(),
        tr("所有矢量格式 (*.shp *.geojson *.gpkg *.kml *.kmz *.dxf *.gml *.tab *.mif);;"
           "Shapefile (*.shp);;"
           "GeoJSON (*.geojson *.json);;"
           "GeoPackage (*.gpkg);;"
           "KML/KMZ (*.kml *.kmz);;"
           "AutoCAD DXF (*.dxf);;"
           "MapInfo (*.tab *.mif);;"
           "所有文件 (*.*)"));
    if (!files.isEmpty())
    {
        emit vectorLayerAddRequested(files);
    }
}

void LayerPanel::onRemoveLayer()
{
    QStringList selectedIds;
    for (auto* item : mLayerTree->selectedItems())
    {
        selectedIds << item->data(0, Qt::UserRole).toString();
    }
    if (selectedIds.isEmpty()) return;

    emit layerRemoveRequested(selectedIds);
}

void LayerPanel::onMoveLayerUp()
{
    QTreeWidgetItem* item = mLayerTree->currentItem();
    if (!item) return;
    int idx = mLayerTree->indexOfTopLevelItem(item);
    if (idx <= 0) return;

    mLayerTree->takeTopLevelItem(idx);
    mLayerTree->insertTopLevelItem(idx - 1, item);
    mLayerTree->setCurrentItem(item);
    emit layerOrderChanged(orderedLayerIds());
}

void LayerPanel::onMoveLayerDown()
{
    QTreeWidgetItem* item = mLayerTree->currentItem();
    if (!item) return;
    int idx = mLayerTree->indexOfTopLevelItem(item);
    if (idx >= mLayerTree->topLevelItemCount() - 1) return;

    mLayerTree->takeTopLevelItem(idx);
    mLayerTree->insertTopLevelItem(idx + 1, item);
    mLayerTree->setCurrentItem(item);
    emit layerOrderChanged(orderedLayerIds());
}

void LayerPanel::onZoomToLayer()
{
    QString id = currentLayerId();
    if (!id.isEmpty())
    {
        emit zoomToLayerRequested(id);
    }
}

void LayerPanel::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (column != 0) return;
    QString id = item->data(0, Qt::UserRole).toString();
    bool visible = (item->checkState(0) == Qt::Checked);
    emit layerVisibilityChanged(id, visible);
}

void LayerPanel::onItemSelectionChanged()
{
    QTreeWidgetItem* item = mLayerTree->currentItem();
    if (item)
    {
        emit layerSelectionChanged(item->data(0, Qt::UserRole).toString());
    }
}

void LayerPanel::onDragOrderChanged()
{
    emit layerOrderChanged(orderedLayerIds());
}

void LayerPanel::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = mLayerTree->itemAt(pos);
    if (!item) return;

    QString layerId = item->data(0, Qt::UserRole).toString();
    mContextLayerId = layerId;
    bool isVector = layerId.startsWith(QStringLiteral("vec_"));

    QMenu menu(this);
    QAction* actZoom      = menu.addAction(tr("缩放到图层"));
    menu.addSeparator();
    QAction* actStyle     = nullptr;
    if (isVector)
    {
        actStyle = menu.addAction(tr("样式设置..."));
        menu.addSeparator();
    }
    QAction* actExport    = menu.addAction(tr("导出图层..."));
    menu.addSeparator();
    QAction* actRemove    = menu.addAction(tr("移除图层"));
    menu.addSeparator();
    QAction* actUp        = menu.addAction(tr("上移"));
    QAction* actDown      = menu.addAction(tr("下移"));

    QAction* selected = menu.exec(mLayerTree->viewport()->mapToGlobal(pos));
    if (selected == actZoom)
    {
        emit zoomToLayerRequested(layerId);
    }
else if (selected == actStyle)
{
    emit vectorStyleRequested(layerId);
    }
else if (selected == actRemove)
{
    emit layerRemoveRequested({ layerId });
    }
else if (selected == actUp)
{
    onMoveLayerUp();
    }
else if (selected == actDown)
{
    onMoveLayerDown();
    }
else if (selected == actExport)
{
    emit exportLayerRequested(layerId);
    }
}

void LayerPanel::setContextBandInfo(const QString& layerId, int bandCount)
{
    // 缓存波段信息，供控制器查询使用
    Q_UNUSED(layerId);
    mContextBandCount = bandCount;
}

void LayerPanel::syncOpacity(double opacity)
{
    if (mOpacitySlider)
    {
        QSignalBlocker b(mOpacitySlider);
        mOpacitySlider->setValue(static_cast<int>(opacity * 100.0));
        if (mOpacityLabel)
            mOpacityLabel->setText(QStringLiteral("%1%").arg(static_cast<int>(opacity * 100.0)));
    }
}
