#ifndef MAPCANVASWIDGET_H
#define MAPCANVASWIDGET_H

#include <QWidget>

class QgsMapCanvas;
class QgsMapToolPan;
class QgsMapToolZoom;
class QgsRectangle;
class QgsPointXY;
class QgsCoordinateReferenceSystem;
class QRectF;
class QToolBar;
class ClickMapTool;

/**
 * @brief QGIS 地图画布封装控件（纯表示层）
 *
 * 包装 QgsMapCanvas，提供缩放/平移/点击交互信号和业务层控制槽。
 * 不含任何坐标系选择、初始范围计算等业务决策。
 */
class MapCanvasWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MapCanvasWidget(QWidget* parent = nullptr);

    QgsMapCanvas* mapCanvas() const;

    /** 返回内置导航工具栏 */
    QToolBar* navToolBar() const;

signals:
    void canvasExtentChanged(const QgsRectangle& extent);
    void mapClicked(const QgsPointXY& point);
    void mapRightClicked(const QgsPointXY& point);
    void spectralPickRequested(const QgsPointXY& geoPoint);

public slots:
    void setCanvasExtent(const QgsRectangle& extent);
    void setCanvasExtent(const QRectF& extent);
    void setCanvasCrs(const QgsCoordinateReferenceSystem& crs);
    void setCanvasColor(const QColor& color);
    void refreshCanvas();

    void zoomIn();
    void zoomOut();
    void zoomToFullExtent();
    void activatePanTool();
    void activateZoomInTool();
    void deactivateSpectralTool();

private slots:
    void onExtentChanged();

private:
    void setupUI();
    void setupMapTools();
    QgsMapCanvas* mCanvas;
    QToolBar* mNavToolBar;
    QgsMapToolPan* mPanTool = nullptr;
    QgsMapToolZoom* mZoomInTool  = nullptr;
    QgsMapToolZoom* mZoomOutTool = nullptr;
    ClickMapTool* mSpectralTool = nullptr;
    QAction* mSpectralAction = nullptr;
};

#endif // MAPCANVASWIDGET_H
