#ifndef LAYERPANEL_H
#define LAYERPANEL_H

#include <QWidget>
#include <QStringList>
#include <QTreeWidget>
#include <QDropEvent>
#include <functional>

class QTreeWidgetItem;
class QToolBar;
class QAction;
class QSlider;
class QLabel;

/**
 * @brief 专用于图层列表的 QTreeWidget 子类（无 Q_OBJECT，使用 std::function 回调）
 *
 * 重写 dropEvent，确保拖拽排序始终以平级方式执行，
 * 避免 InternalMove 模式将图层错误地嵌套为子节点。
 */
class LayerTreeWidget : public QTreeWidget
{
public:
    explicit LayerTreeWidget(QWidget* parent = nullptr)
    : QTreeWidget(parent)
    {
        setDragDropMode(QAbstractItemView::InternalMove);
        setDefaultDropAction(Qt::MoveAction);
        setDragEnabled(true);
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        setRootIsDecorated(false);
    }

    void setOnOrderChanged(std::function<void()> callback)
    {
        mOnOrderChanged = std::move(callback);
    }

protected:
    void dropEvent(QDropEvent* event) override
    {
        QStringList idsBefore;
        for (int i = 0; i < topLevelItemCount(); ++i)
            idsBefore << topLevelItem(i)->data(0, Qt::UserRole).toString();

        // 必须调用基类：InternalMove 模式下负责解码 MIME 并重建 item
        QTreeWidget::dropEvent(event);

        if (!event->isAccepted()) return;

        // 修正：将任何被错误嵌套为子节点的图层提取回顶层
        for (int i = 0; i < topLevelItemCount(); ++i)
        {
            QTreeWidgetItem* parentItem = topLevelItem(i);
            while (parentItem->childCount() > 0)
            {
                QTreeWidgetItem* child = parentItem->takeChild(0);
                insertTopLevelItem(i + 1, child);
            }
        }

        QStringList idsAfter;
        for (int i = 0; i < topLevelItemCount(); ++i)
            idsAfter << topLevelItem(i)->data(0, Qt::UserRole).toString();

        if (idsBefore != idsAfter && mOnOrderChanged)
            mOnOrderChanged();
    }

private:
    std::function<void()> mOnOrderChanged;
};

/**
 * @brief 图层管理面板（纯表示层，左侧停靠）
 */
class LayerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LayerPanel(QWidget* parent = nullptr);

    int layerCount() const;
    QString currentLayerId() const;

signals:
    void layerAddRequested(const QStringList& filePaths);
    void layerRemoveRequested(const QStringList& layerIds);
    void layerVisibilityChanged(const QString& layerId, bool visible);
    void layerOrderChanged(const QStringList& orderedIds);
    void zoomToLayerRequested(const QString& layerId);
    void layerSelectionChanged(const QString& layerId);
    void splitBandsRequested(const QString& layerId);
    void bandCombinationRequested(const QString& layerId);
    void bandManagerRequested(const QString& layerId);
    void bandManagerToggleRequested();
    void exportLayerRequested(const QString& layerId);
    void opacityChanged(const QString& layerId, double opacity);

public slots:
    void onLayerLoaded(const QString& layerId, const QString& name, const QString& type);
    void onLayerRemoved(const QString& layerId);
    void onLayerError(const QString& layerId, const QString& errorMsg);
    void onLayerVisibilitySet(const QString& layerId, bool visible);

    void setContextBandInfo(const QString& layerId, int bandCount);
    void syncOpacity(double opacity);

public slots:
    void onAddLayer();

private slots:
    void onRemoveLayer();
    void onMoveLayerUp();
    void onMoveLayerDown();
    void onZoomToLayer();
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onItemSelectionChanged();
    void onContextMenu(const QPoint& pos);
    void onDragOrderChanged();

private:
    void setupUI();
    void createActions();
    QTreeWidgetItem* findItem(const QString& layerId) const;
    QStringList orderedLayerIds() const;

    LayerTreeWidget* mLayerTree;
    QToolBar* mToolBar;

    QAction* mAddAction;
    QAction* mRemoveAction;
    QAction* mMoveUpAction;
    QAction* mMoveDownAction;
    QAction* mZoomToAction;
    QAction* mBandManagerAction;

    QSlider*    mOpacitySlider = nullptr;
    QLabel*     mOpacityLabel  = nullptr;

    // 上次右键的 layerId
    QString mContextLayerId;
    int     mContextBandCount = 0;
};

#endif // LAYERPANEL_H
