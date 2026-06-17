#include "MapCanvasWidget.h"

#include <qgsmapcanvas.h>
#include <qgsmaptoolpan.h>
#include <qgsmaptoolzoom.h>
#include <qgsrectangle.h>
#include <qgspointxy.h>
#include <qgscoordinatereferencesystem.h>
#include <qgsmapmouseevent.h>

#include <QVBoxLayout>
#include <QToolBar>
#include <QAction>
#include <functional>

// ─── Click-to-signal map tool (no Q_OBJECT, uses std::function callbacks) ────

class ClickMapTool : public QgsMapTool
{
public:
    using ClickCallback = std::function<void(const QgsPointXY&)>;

    ClickMapTool(QgsMapCanvas* canvas, ClickCallback leftCb, ClickCallback rightCb)
        : QgsMapTool(canvas), mLeftCb(std::move(leftCb)), mRightCb(std::move(rightCb)) {}

    void canvasReleaseEvent(QgsMapMouseEvent* e) override
    {
        QgsPointXY pt = toMapCoordinates(e->pos());
        if (e->button() == Qt::LeftButton && mLeftCb)
            mLeftCb(pt);
        else if (e->button() == Qt::RightButton && mRightCb)
            mRightCb(pt);
    }

private:
    ClickCallback mLeftCb;
    ClickCallback mRightCb;
};

// ─── MapCanvasWidget ──────────────────────────────────────────────────────

MapCanvasWidget::MapCanvasWidget(QWidget* parent)
    : QWidget(parent)
    {
    setupUI();
    setupMapTools();
}

void MapCanvasWidget::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 导航工具栏
    mNavToolBar = new QToolBar(this);
    mNavToolBar->setIconSize(QSize(18, 18));
    mNavToolBar->setMovable(false);
    layout->addWidget(mNavToolBar);

    // 地图画布
    mCanvas = new QgsMapCanvas(this);
    mCanvas->setCanvasColor(QColor(255, 255, 255));
    mCanvas->setParallelRenderingEnabled(true);
    mCanvas->setCachingEnabled(true);
    mCanvas->setMinimumSize(200, 200);

    layout->addWidget(mCanvas);

    connect(mCanvas, &QgsMapCanvas::extentsChanged, this, &MapCanvasWidget::onExtentChanged);
}

void MapCanvasWidget::setupMapTools()
{
    // 平移工具（默认激活）
    mPanTool = new QgsMapToolPan(mCanvas);

    // 放大工具
    mZoomInTool = new QgsMapToolZoom(mCanvas, false);

    // 缩小工具
    mZoomOutTool = new QgsMapToolZoom(mCanvas, true);

    // 点击识别工具 — 通过回调转发为 MapCanvasWidget 信号
    auto* clickTool = new ClickMapTool(mCanvas,
        [this](const QgsPointXY& pt) { emit mapClicked(pt); },
        [this](const QgsPointXY& pt) { emit mapRightClicked(pt); });

    // 默认激活平移
    mCanvas->setMapTool(mPanTool);

    // ── 工具栏按钮 ──
    QAction* actPan = mNavToolBar->addAction(tr("平移"));
    actPan->setCheckable(true);
    actPan->setChecked(true);
    connect(actPan, &QAction::triggered, this, [this]()
    {
        mCanvas->setMapTool(mPanTool);
    });

    QAction* actZoomIn = mNavToolBar->addAction(tr("放大"));
    actZoomIn->setCheckable(true);
    connect(actZoomIn, &QAction::triggered, this, [this]()
    {
        mCanvas->setMapTool(mZoomInTool);
    });

    QAction* actZoomOut = mNavToolBar->addAction(tr("缩小"));
    actZoomOut->setCheckable(true);
    connect(actZoomOut, &QAction::triggered, this, [this]()
    {
        mCanvas->setMapTool(mZoomOutTool);
    });

    QAction* actClick = mNavToolBar->addAction(tr("点选"));
    actClick->setToolTip(QStringLiteral("点选查询坐标"));
    actClick->setCheckable(true);
    connect(actClick, &QAction::triggered, this, [this, clickTool]()
    {
        mCanvas->setMapTool(clickTool);
    });

    mNavToolBar->addSeparator();

    QAction* actZoomFull = mNavToolBar->addAction(tr("全图"));
    actZoomFull->setToolTip(QStringLiteral("缩放到全图范围"));
    connect(actZoomFull, &QAction::triggered, this, &MapCanvasWidget::zoomToFullExtent);

    // Spectral pick tool
    mSpectralTool = new ClickMapTool(mCanvas,
        [this](const QgsPointXY& pt) { emit spectralPickRequested(pt); },
        nullptr);

    mSpectralAction = mNavToolBar->addAction(tr("Spectral"));
    mSpectralAction->setToolTip(tr("Spectral profile picker: click on map to extract spectrum"));
    mSpectralAction->setCheckable(true);
    connect(mSpectralAction, &QAction::toggled, this, [this](bool checked) {
        if (checked)
            mCanvas->setMapTool(mSpectralTool);
        else
            mCanvas->setMapTool(mPanTool);
    });
}

QgsMapCanvas* MapCanvasWidget::mapCanvas() const
{
    return mCanvas;
}

QToolBar* MapCanvasWidget::navToolBar() const
{
    return mNavToolBar;
}

// ─── Private slot ────────────────────────────────────────────────────────

void MapCanvasWidget::onExtentChanged()
{
    emit canvasExtentChanged(mCanvas->extent());
}

// ─── Public slots ────────────────────────────────────────────────────────

void MapCanvasWidget::setCanvasExtent(const QgsRectangle& extent)
{
    mCanvas->setExtent(extent);
    mCanvas->refresh();
}

void MapCanvasWidget::setCanvasExtent(const QRectF& extent)
{
    mCanvas->setExtent(QgsRectangle(extent.left(), extent.bottom(),
                                     extent.right(), extent.top()));
    mCanvas->refresh();
}

void MapCanvasWidget::setCanvasCrs(const QgsCoordinateReferenceSystem& crs)
{
    mCanvas->setDestinationCrs(crs);
    mCanvas->refresh();
}

void MapCanvasWidget::setCanvasColor(const QColor& color)
{
    mCanvas->setCanvasColor(color);
    mCanvas->refresh();
}

void MapCanvasWidget::refreshCanvas()
{
    mCanvas->refresh();
}

void MapCanvasWidget::zoomIn()
{
    mCanvas->zoomIn();
}

void MapCanvasWidget::zoomOut()
{
    mCanvas->zoomOut();
}

void MapCanvasWidget::zoomToFullExtent()
{
    mCanvas->zoomToFullExtent();
}

void MapCanvasWidget::activatePanTool()
{
    mCanvas->setMapTool(mPanTool);
}

void MapCanvasWidget::activateZoomInTool()
{
    mCanvas->setMapTool(mZoomInTool);
}

void MapCanvasWidget::deactivateSpectralTool()
{
    if (mSpectralAction && mSpectralAction->isChecked())
        mSpectralAction->setChecked(false);
}

