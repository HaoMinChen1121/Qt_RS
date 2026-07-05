#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "SARibbonMainWindow.h"

class SARibbonCategory;
class SARibbonQuickAccessBar;
class SARibbonButtonGroupWidget;
class SARibbonPanel;
class SARibbonGalleryGroup;
class QCloseEvent;
class QLineEdit;

// 表示层控件（前向声明）
class MapCanvasWidget;
class WorkflowPanel;
class BatchProcessPanel;
#include "domain/params/RadiometricCorrectionParams.h"
#include "domain/params/ImageFusionParams.h"
#include "domain/params/MosaicParams.h"

class LayerPanel;
class RasterMetadataPanel;
class VectorMetadataPanel;
class BandManagerPanel;
class QDockWidget;
class QStackedWidget;
class SpectralProfileDialog;

class ApplicationController;

#include "ui/GeometricTypes.h"

namespace Ui
{
class MainWindow;
}

/**
 * @brief 多源遥感影像全流程批处理系统主窗口（表示层）
 * @details 负责 Ribbon UI 构建，包含辐射定标、几何校正、图像融合、
 *          镶嵌成图、工作流设计器、批处理引擎六大功能模块。
 *          仅包含界面构建和用户交互响应。数据访问与业务逻辑不在此层。
 */
class MainWindow : public SARibbonMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* par = nullptr);
    ~MainWindow();

    // 访问器方法（供 ApplicationController 连接信号/槽）
    LayerPanel* layerPanel() const { return mLayerPanel; }
    RasterMetadataPanel* metadataPanel() const { return mMetadataPanel; }
    VectorMetadataPanel* vectorMetadataPanel() const { return mVectorMetadataPanel; }
    BandManagerPanel* bandManagerPanel() const { return mBandManagerPanel; }

    // 元数据面板切换
    void showRasterMetadata();
    void showVectorMetadata();
    MapCanvasWidget* mapCanvasWidget() const { return mMapCanvasWidget; }
    SpectralProfileDialog* spectralDialog() const { return mSpectralDialog; }
    void setAppController(ApplicationController* ctrl) { mAppController = ctrl; }
    ApplicationController* appController() const { return mAppController; }

signals:
    void calibrationRequested(const RadiometricCorrectionParams& params);
    void correctionRequested(const GeometricInput& input);
    void cancelCorrectionRequested();
    void fusionRequested(const ImageFusionParams& params);
    void mosaicRequested(const MosaicParams& params);
    void workflowNewRequested();
    void workflowSaveRequested();
    void workflowLoadRequested();

private:
    void createCategoryFile(SARibbonCategory* page);
    void createCategoryRadiometric(SARibbonCategory* page);
    void createCategoryGeometric(SARibbonCategory* page);
    void createCategoryFusion(SARibbonCategory* page);
    void createCategoryMosaic(SARibbonCategory* page);
    void createCategoryWorkflow(SARibbonCategory* page);
    void createCategoryBatch(SARibbonCategory* page);
    void createQuickAccessBar();
    void createRightButtonGroup();
    void createWindowButtonGroupBar();
    QAction* createAction(const QString& text, const QString& iconurl, const QString& objName);
    QAction* createAction(const QString& text, const QString& iconurl);

    void initUI();
    void initMapCanvas();
    void initLayerPanel();
    void initMetadataPanel();
    void initBandManagerPanel();
    void initSpectralDialog();

    MapCanvasWidget* mMapCanvasWidget = nullptr;
    QDockWidget* mLayerDock = nullptr;
    LayerPanel* mLayerPanel = nullptr;
    QDockWidget* mMetadataDock = nullptr;
    QStackedWidget* mMetadataStack = nullptr;
    RasterMetadataPanel* mMetadataPanel = nullptr;
    VectorMetadataPanel* mVectorMetadataPanel = nullptr;
    BandManagerPanel* mBandManagerPanel = nullptr;

private Q_SLOTS:
    void onActionHelpTriggered();
    void onUndoActionTriggered();
    void onRedoActionTriggered();
    void onSearchEditorEditingFinished();
    void onLoginActionTriggered();

protected:
    void closeEvent(QCloseEvent* closeEvent) override;

private:
    QLineEdit* mSearchEditor { nullptr };

    // 可选模块面板
    WorkflowPanel* mWorkflowPanel = nullptr;
    BatchProcessPanel* mBatchPanel = nullptr;

    // 几何校正：Ribbon 输入缓存（传递给对话框）
    GeometricInput mGeoInput;

    // 镶嵌成图：Ribbon 预选影像缓存
    QStringList mMosaicImages;

    // 辐射定标：记录最后加载的产品路径和传感器类型
    QString mLastOpenedImagePath;
    QString mLastDetectedSensor;

    Ui::MainWindow* ui;

    ApplicationController* mAppController = nullptr;
    SpectralProfileDialog* mSpectralDialog = nullptr;
};

#endif  // MAINWINDOW_H
