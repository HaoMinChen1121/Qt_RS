#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "SARibbonApplicationButton.h"
#include "SARibbonBar.h"
#include "SARibbonButtonGroupWidget.h"
#include "SARibbonCategory.h"
#include "SARibbonMenu.h"
#include "SARibbonPanel.h"
#include "SARibbonQuickAccessBar.h"
#include "SARibbonSystemButtonBar.h"

// 表示层 — 对话框类 (弹出式参数设置)
#include "ui/RadiometricDialog.h"
#include "ui/GeometricDialog.h"
#include "ui/SpectralProfileDialog.h"
#include "ui/FusionDialog.h"
#include "ui/MosaicDialog.h"
// 表示层 — 画布与面板控件
#include "ui/MapCanvasWidget.h"
#include "ui/LayerPanel.h"
#include "ui/RasterMetadataPanel.h"
#include "ui/BandManagerPanel.h"
// 表示层 — 仅工作流/批处理保留嵌入式面板
#include "ui/WorkflowPanel.h"
#include "ui/BatchProcessPanel.h"

#include "dataaccess/ISensorProduct.h"
#include "dataaccess/SensorProductFactory.h"
#include <QAbstractButton>
#include <QAction>
#include <QComboBox>
#include <QToolButton>
#include <memory>
#include <QDockWidget>
#include <QFileDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>

MainWindow::MainWindow(QWidget* parent) : SARibbonMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initUI();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initUI()
{
    setWindowTitle(("多源遥感影像全流程批处理系统[*]"));
    setWindowModified(true);

    initMapCanvas();
    initLayerPanel();
    initMetadataPanel();
    initBandManagerPanel();
    initSpectralDialog();

    SARibbonBar* ribbonBar = this->ribbonBar();
    setContentsMargins(1, 0, 1, 0);
    ribbonBar->setContentsMargins(4, 0, 4, 0);

    // === 创建功能分类页 ===
    SARibbonCategory* catFile = ribbonBar->addCategoryPage(tr("文件"));
    catFile->setObjectName("catFile");
    createCategoryFile(catFile);

    SARibbonCategory* catRadiometric = ribbonBar->addCategoryPage(tr("辐射定标"));
    catRadiometric->setObjectName("catRadiometric");
    createCategoryRadiometric(catRadiometric);

    SARibbonCategory* catGeometric = ribbonBar->addCategoryPage(tr("几何校正"));
    catGeometric->setObjectName("catGeometric");
    createCategoryGeometric(catGeometric);

    SARibbonCategory* catFusion = ribbonBar->addCategoryPage(tr("图像融合"));
    catFusion->setObjectName("catFusion");
    createCategoryFusion(catFusion);

    SARibbonCategory* catMosaic = ribbonBar->addCategoryPage(tr("镶嵌成图"));
    catMosaic->setObjectName("catMosaic");
    createCategoryMosaic(catMosaic);

    SARibbonCategory* catWorkflow = ribbonBar->addCategoryPage(tr("工作流"));
    catWorkflow->setObjectName("catWorkflow");
    createCategoryWorkflow(catWorkflow);

    SARibbonCategory* catBatch = ribbonBar->addCategoryPage(tr("批处理"));
    catBatch->setObjectName("catBatch");
    createCategoryBatch(catBatch);

    createQuickAccessBar();
    createRightButtonGroup();
    createWindowButtonGroupBar();

    setMinimumWidth(800);
    setWindowIcon(QIcon(":/icon/icon/SA.svg"));

    setRibbonTheme(SARibbonTheme::RibbonThemeOffice2021Blue);
    ribbonBar->setRibbonStyle(SARibbonBar::RibbonStyleLooseThreeRow);

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QTimer::singleShot(0, this, &QWidget::showMaximized);
#else
    showMaximized();
#endif
}

void MainWindow::initMapCanvas()
{
    mMapCanvasWidget = new MapCanvasWidget(this);
    this->setCentralWidget(mMapCanvasWidget);

    // 用户缩放/平移画布 → 通知业务逻辑层
    connect(mMapCanvasWidget, &MapCanvasWidget::canvasExtentChanged, this, [this](const QgsRectangle& extent)
    {
        // TODO: 通知业务逻辑层当前视口范围已变更
        Q_UNUSED(extent);
    });

    // 初始 CRS 与范围由业务逻辑层加载数据后通过槽设置
    // mMapCanvasWidget->setCanvasCrs(...)
    // mMapCanvasWidget->setCanvasExtent(...)
}

void MainWindow::initLayerPanel()
{
    mLayerDock = new QDockWidget(tr("图层"), this);
    mLayerDock->setObjectName("LayerDock");
    mLayerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    mLayerDock->setMinimumWidth(200);

    mLayerPanel = new LayerPanel(mLayerDock);
    mLayerDock->setWidget(mLayerPanel);

    addDockWidget(Qt::LeftDockWidgetArea, mLayerDock);
}

void MainWindow::initMetadataPanel()
{
    mMetadataDock = new QDockWidget(tr("影像元数据"), this);
    mMetadataDock->setObjectName("MetadataDock");
    mMetadataDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    mMetadataDock->setFeatures(QDockWidget::DockWidgetMovable |
                                QDockWidget::DockWidgetFloatable);
    mMetadataDock->setMinimumWidth(180);
    mMetadataDock->setMaximumWidth(500);

    mMetadataPanel = new RasterMetadataPanel(mMetadataDock);
    mMetadataDock->setWidget(mMetadataPanel);

    addDockWidget(Qt::RightDockWidgetArea, mMetadataDock);

    // Give the metadata dock a modest default share — canvas gets the rest
    resizeDocks({mMetadataDock}, {220}, Qt::Horizontal);
}

void MainWindow::initBandManagerPanel()
{
    mBandManagerPanel = new BandManagerPanel(this);
    mBandManagerPanel->setObjectName(QStringLiteral("BandManagerPanel"));
    mBandManagerPanel->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea
                                       | Qt::BottomDockWidgetArea);
    mBandManagerPanel->setFeatures(QDockWidget::DockWidgetMovable
                                   | QDockWidget::DockWidgetFloatable
                                   | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::LeftDockWidgetArea, mBandManagerPanel);
    mBandManagerPanel->hide();  // hidden by default, toggled via toolbar button
}

void MainWindow::initSpectralDialog()
{
    mSpectralDialog = new SpectralProfileDialog(this);
    connect(mSpectralDialog, &SpectralProfileDialog::closed,
        mMapCanvasWidget, &MapCanvasWidget::deactivateSpectralTool);
}

// ========================================================================
// Category 0: 文件 — 按传感器类型打开影像产品
// ========================================================================
void MainWindow::createCategoryFile(SARibbonCategory* page)
{
    // --- Panel 1: 按传感器打开产品 ---
    SARibbonPanel* pnlSensor = page->addPanel(tr("打开传感器影像"));

    QAction* actOpenS2 = createAction(tr("Sentinel-2\n(ZIP/SAFE)"), ":/icon/icon/folder-star.svg", "actOpenS2");
    pnlSensor->addLargeAction(actOpenS2);
    connect(actOpenS2, &QAction::triggered, this, [this]()
    {
        QString file = QFileDialog::getOpenFileName(this, tr("选择 Sentinel-2 产品"),
            QString(), tr("Sentinel-2 产品 (*.zip *.rpp *.SAFE);;所有文件 (*.*)"));
        if (!file.isEmpty() && mLayerPanel)
            emit mLayerPanel->layerAddRequested({file});
    });

    QAction* actOpenLS = createAction(tr("Landsat\n(MTL/TIF)"), ":/icon/icon/folder-checkmark.svg", "actOpenLS");
    pnlSensor->addLargeAction(actOpenLS);
    connect(actOpenLS, &QAction::triggered, this, [this]()
    {
        QString file = QFileDialog::getOpenFileName(this, tr("选择 Landsat 元数据文件"),
            QString(), tr("Landsat 元数据 (*_MTL.txt);;所有文件 (*.*)"));
        if (!file.isEmpty() && mLayerPanel)
            emit mLayerPanel->layerAddRequested({file});
    });

    QAction* actOpenGf = createAction(tr("高分系列\n(GF-1/2/6)"), ":/icon/icon/folder-table.svg", "actOpenGf");
    pnlSensor->addLargeAction(actOpenGf);
    connect(actOpenGf, &QAction::triggered, this, [this]()
    {
        QString dir = QFileDialog::getExistingDirectory(this, tr("选择高分影像目录"));
        if (!dir.isEmpty() && mLayerPanel)
            emit mLayerPanel->layerAddRequested({dir});
    });

    // --- Panel 2: 通用 ---
    SARibbonPanel* pnlGeneral = page->addPanel(tr("通用"));

    QAction* actOpenFile = createAction(tr("添加通用\n栅格图层"), ":/icon/icon/save.svg", "actAddRaster");
    pnlGeneral->addLargeAction(actOpenFile);
    connect(actOpenFile, &QAction::triggered, this, [this]()
    {
        if (mLayerPanel)
            mLayerPanel->onAddLayer();
    });

    pnlSensor->setVisible(true);
    pnlGeneral->setVisible(true);
}

// ========================================================================
// Category 1: 辐射定标与大气校正
// ========================================================================
void MainWindow::createCategoryRadiometric(SARibbonCategory* page)
{
    // --- Panel 1: 传感器与输入 ---
    SARibbonPanel* pnlInput = page->addPanel(tr("传感器与输入"));

    auto currentSensor = std::make_shared<QString>("自动识别");

    // "打开影像" 分裂按钮 — 类似 ENVI "Open As"：点击主按钮用上次传感器类型打开，下拉菜单切换传感器
    QToolButton* btnOpenImage = new QToolButton(this);
    btnOpenImage->setText(tr("打开影像"));
    btnOpenImage->setIcon(QIcon(":/icon/icon/save.svg"));
    btnOpenImage->setToolTip(tr("以指定传感器类型打开影像"));
    btnOpenImage->setPopupMode(QToolButton::MenuButtonPopup);
    btnOpenImage->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QMenu* sensorMenu = new QMenu(btnOpenImage);
    QActionGroup* sensorGroup = new QActionGroup(sensorMenu);
    sensorGroup->setExclusive(true);

    auto addSensorItem = [&](const QString& name, bool checked)
    {
        QAction* act = sensorMenu->addAction(name);
        act->setCheckable(true);
        sensorGroup->addAction(act);
        if (checked) act->setChecked(true);
    };
    addSensorItem("自动识别", true);
    sensorMenu->addSeparator();
    addSensorItem("Landsat-8", false);
    addSensorItem("Landsat-9", false);
    addSensorItem("Sentinel-2A", false);
    addSensorItem("Sentinel-2B", false);
    addSensorItem("GF-1", false);
    addSensorItem("GF-2", false);
    addSensorItem("GF-6", false);

    btnOpenImage->setMenu(sensorMenu);

    auto openImageAsSensor = [this, currentSensor](const QString& sensor)
    {
        *currentSensor = sensor;
        QString path;
        if (sensor.startsWith("Sentinel-2"))
            path = QFileDialog::getOpenFileName(this, tr("选择 Sentinel-2 产品"),
                QString(), tr("Sentinel-2 产品 (*.zip *.rpp *.SAFE);;所有文件 (*.*)"));
        else if (sensor.startsWith("Landsat"))
            path = QFileDialog::getOpenFileName(this, tr("选择 Landsat 元数据"),
                QString(), tr("Landsat 元数据 (*_MTL.txt);;所有文件 (*.*)"));
        else if (sensor.startsWith("GF-"))
            path = QFileDialog::getExistingDirectory(this, tr("选择高分影像目录"));
        else
            path = QFileDialog::getOpenFileName(this, tr("选择影像文件"),
                QString(), tr("遥感影像 (*.tif *.tiff *.img *.zip *.rpp *.SAFE);;所有文件 (*.*)"));
        if (path.isEmpty())
            return;

        // 自动检测实际传感器类型，供后续"执行定标"预填充
        mLastOpenedImagePath = path;
        QScopedPointer<ISensorProduct> prod(createSensorProduct(path));
        if (prod && prod->open(path))
            mLastDetectedSensor = prod->sensorType();
        else
            mLastDetectedSensor = sensor;

        if (mLayerPanel)
            emit mLayerPanel->layerAddRequested({path});
    };

    connect(btnOpenImage, &QToolButton::clicked, this, [openImageAsSensor, currentSensor]()
    {
        openImageAsSensor(*currentSensor);
    });

    connect(sensorMenu, &QMenu::triggered, this, [openImageAsSensor](QAction* act)
    {
        openImageAsSensor(act->text());
    });

    pnlInput->addSmallWidget(btnOpenImage);

    QAction* actMeta = createAction(tr("元数据"), ":/icon/icon/file.svg", "actMeta");
    pnlInput->addSmallAction(actMeta);
    connect(actMeta, &QAction::triggered, this, [this, currentSensor]()
    {
        QString sensor = *currentSensor;
        QString path;
        if (sensor.startsWith("Sentinel-2"))
            path = QFileDialog::getOpenFileName(this, tr("选择 Sentinel-2 产品"),
                QString(), tr("Sentinel-2 产品 (*.zip *.rpp *.SAFE);;所有文件 (*.*)"));
        else if (sensor.startsWith("Landsat"))
            path = QFileDialog::getOpenFileName(this, tr("选择 Landsat 元数据"),
                QString(), tr("Landsat 元数据 (*_MTL.txt);;所有文件 (*.*)"));
        else
            path = QFileDialog::getOpenFileName(this, tr("选择元数据文件"),
                QString(), tr("元数据文件 (*.xml *.txt);;所有文件 (*.*)"));
        if (path.isEmpty()) return;
        QScopedPointer<ISensorProduct> prod(createSensorProduct(path));
        if (!prod || !prod->open(path))
        {
            QMessageBox::warning(this, tr("错误"), tr("无法识别此产品的元数据"));
            return;
        }
        SensorInfo info = prod->sensorInfo();
        QString msg = tr("传感器: %1\n波段数: %2\n采集时间: %3\n"
                         "太阳天顶角: %4°\n太阳方位角: %5°\n"
                         "观测天顶角: %6°\n观测方位角: %7°\n日地距离: %8 AU")
            .arg(info.sensorId).arg(info.bands.size())
            .arg(info.acquisitionTime.toString("yyyy-MM-dd hh:mm"))
            .arg(info.solarZenithAngle, 0, 'f', 1)
            .arg(info.solarAzimuthAngle, 0, 'f', 1)
            .arg(info.sensorZenithAngle, 0, 'f', 1)
            .arg(info.sensorAzimuthAngle, 0, 'f', 1)
            .arg(info.earthSunDistance, 0, 'f', 4);
        if (info.meanAOT > 0.0)
            msg += tr("\n大气 AOT (550nm): %1").arg(info.meanAOT, 0, 'f', 4);
        if (info.meanWV > 0.0)
            msg += tr("\n水汽含量: %1 g/cm²").arg(info.meanWV, 0, 'f', 3);
        QMessageBox::information(this, tr("元数据信息"), msg);
    });

    // --- Panel 2: 标定与校正 ---
    SARibbonPanel* pnlCal = page->addPanel(tr("标定与校正"));

    QComboBox* calTypeCombo = new QComboBox(this);
    calTypeCombo->addItems({ tr("DN→辐亮度"), tr("DN→反射率") });
    pnlCal->addSmallWidget(calTypeCombo);

    QComboBox* atmCombo = new QComboBox(this);
    atmCombo->addItems({ "6S", "Py6S", "Sen2Cor", tr("无") });
    pnlCal->addSmallWidget(atmCombo);

    QAction* actBatch = createAction(tr("批量模式"), ":/icon/icon/folder-stats.svg", "actBatch");
    actBatch->setCheckable(true);
    pnlCal->addSmallAction(actBatch);

    // --- Panel 3: 执行 ---
    SARibbonPanel* pnlExec = page->addPanel(tr("执行"));

    QAction* actExecute = createAction(tr("执行定标\n与校正"), ":/icon/icon/folder-cog.svg", "actExecCal");
    pnlExec->addLargeAction(actExecute);
    connect(actExecute, &QAction::triggered, this, [this, currentSensor, calTypeCombo, atmCombo]()
    {
        RadiometricDialog dlg(this);
        RadiometricCorrectionParams p;
        // 优先使用自动检测的传感器类型，其次使用下拉框选择，最后回退 Landsat-8
        if (!mLastDetectedSensor.isEmpty())
            p.sensorType = mLastDetectedSensor;
        else if (*currentSensor != "自动识别")
            p.sensorType = *currentSensor;
        else
            p.sensorType = "Landsat-8";
        p.calibrationType = calTypeCombo->currentIndex() == 0 ? "DN2Radiance" : "DN2Reflectance";
        QString atm = atmCombo->currentText();
        p.atmModel = (atm == "6S") ? "6S" : (atm == "Py6S") ? "Py6S" : (atm.contains("Sen2Cor")) ? "Sen2Cor" : "None";
        if (!mLastOpenedImagePath.isEmpty())
        {
            p.metadataFile = mLastOpenedImagePath;
            // If product container (SAFE/ZIP), resolve to individual band raster files
            QScopedPointer<ISensorProduct> prod(createSensorProduct(mLastOpenedImagePath));
            if (prod && prod->open(mLastOpenedImagePath))
            {
                SensorInfo info = prod->sensorInfo();
                p.solarZenithAngle = info.solarZenithAngle;
                p.solarAzimuthAngle = info.solarAzimuthAngle;
                p.earthSunDistance = info.earthSunDistance;
                p.sensorZenithAngle = info.sensorZenithAngle;
                p.sensorAzimuthAngle = info.sensorAzimuthAngle;
                if (!info.sensorType.isEmpty())
                    p.sensorType = info.sensorType;
                const auto bands = prod->bands();
                for (const auto& b : bands)
                    p.inputFiles.append(b.rasterPath);
                dlg.setSensorInfo(info);
            }
            else
            {
                p.inputFiles.append(mLastOpenedImagePath);
            }
        }
        dlg.setParams(p);
        if (dlg.exec() == QDialog::Accepted)
        {
            emit calibrationRequested(dlg.params());
        }
    });

    QAction* actAdvanced = createAction(tr("高级参数"), ":/icon/icon/layout.svg", "actAdvCal");
    pnlExec->addSmallAction(actAdvanced);
    connect(actAdvanced, &QAction::triggered, this, [this, currentSensor, calTypeCombo, atmCombo]()
    {
        RadiometricDialog dlg(this);
        RadiometricCorrectionParams p;
        if (!mLastDetectedSensor.isEmpty())
            p.sensorType = mLastDetectedSensor;
        else if (*currentSensor != "自动识别")
            p.sensorType = *currentSensor;
        else
            p.sensorType = "Landsat-8";
        p.calibrationType = calTypeCombo->currentIndex() == 0 ? "DN2Radiance" : "DN2Reflectance";
        QString atm = atmCombo->currentText();
        p.atmModel = (atm == "6S") ? "6S" : (atm == "Py6S") ? "Py6S" : (atm.contains("Sen2Cor")) ? "Sen2Cor" : "None";
        if (!mLastOpenedImagePath.isEmpty())
        {
            p.metadataFile = mLastOpenedImagePath;
            QScopedPointer<ISensorProduct> prod(createSensorProduct(mLastOpenedImagePath));
            if (prod && prod->open(mLastOpenedImagePath))
            {
                SensorInfo info = prod->sensorInfo();
                p.solarZenithAngle = info.solarZenithAngle;
                p.solarAzimuthAngle = info.solarAzimuthAngle;
                p.earthSunDistance = info.earthSunDistance;
                p.sensorZenithAngle = info.sensorZenithAngle;
                p.sensorAzimuthAngle = info.sensorAzimuthAngle;
                if (!info.sensorType.isEmpty())
                    p.sensorType = info.sensorType;
                const auto bands = prod->bands();
                for (const auto& b : bands)
                    p.inputFiles.append(b.rasterPath);
                dlg.setSensorInfo(info);
            }
            else
            {
                p.inputFiles.append(mLastOpenedImagePath);
            }
        }
        dlg.setParams(p);
        if (dlg.exec() == QDialog::Accepted)
            emit calibrationRequested(dlg.params());
    });

    pnlInput->setVisible(true);
    pnlCal->setVisible(true);
    pnlExec->setVisible(true);
}

// ========================================================================
// Category 2: 几何精校正
// ========================================================================
void MainWindow::createCategoryGeometric(SARibbonCategory* page)
{
    // --- Panel 1: 输入数据 ---
    SARibbonPanel* pnlInput = page->addPanel(tr("输入数据"));

    QAction* actSrcImg = createAction(tr("待校正\n影像"), ":/icon/icon/save.svg", "actGeoSrcImg");
    pnlInput->addLargeAction(actSrcImg);
    connect(actSrcImg, &QAction::triggered, this, [this]()
    {
        QString file = QFileDialog::getOpenFileName(this, tr("选择待校正影像"),
            QString(), tr("遥感影像 (*.tif *.tiff *.img);;所有文件 (*.*)"));
        if (!file.isEmpty())
            mGeoInput.sourceImage = file;
    });

    QAction* actRefImg = createAction(tr("参考\n影像"), ":/icon/icon/Align-Left.svg", "actGeoRefImg");
    pnlInput->addLargeAction(actRefImg);
    connect(actRefImg, &QAction::triggered, this, [this]()
    {
        QString file = QFileDialog::getOpenFileName(this, tr("选择参考影像"),
            QString(), tr("遥感影像 (*.tif *.tiff *.img);;所有文件 (*.*)"));
        if (!file.isEmpty())
            mGeoInput.referenceImage = file;
    });

    // --- Panel 2: 匹配设置 ---
    SARibbonPanel* pnlMatch = page->addPanel(tr("匹配设置"));

    QComboBox* algoCombo = new QComboBox(this);
    algoCombo->addItems({"NCC", "SIFT", "SURF", "Harris"});
    algoCombo->setToolTip(tr("匹配算法"));
    pnlMatch->addSmallWidget(algoCombo);
    connect(algoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int idx) {
            static const char* algos[] = {"NCC", "SIFT", "SURF", "HarrisCorner"};
            mGeoInput.matchingAlgorithm = algos[idx];
        });
    mGeoInput.matchingAlgorithm = "NCC";

    QComboBox* modeCombo = new QComboBox(this);
    modeCombo->addItems({tr("自动匹配"), tr("半自动匹配"), tr("手动选点")});
    modeCombo->setToolTip(tr("匹配模式"));
    pnlMatch->addSmallWidget(modeCombo);
    connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int idx) {
            static const char* modes[] = {"Auto", "SemiAuto", "Manual"};
            mGeoInput.matchingMode = modes[idx];
        });
    mGeoInput.matchingMode = "Auto";

    // --- Panel 3: 执行 ---
    SARibbonPanel* pnlExec = page->addPanel(tr("校正执行"));

    QAction* actExecute = createAction(tr("执行\n校正"), ":/icon/icon/folder-cog.svg", "actGeoExec");
    pnlExec->addLargeAction(actExecute);
    connect(actExecute, &QAction::triggered, this, [this]()
    {
        if (mGeoInput.sourceImage.isEmpty())
        {
            QMessageBox::information(this, tr("提示"), tr("请先选择待校正影像"));
            return;
        }
        GeometricDialog dlg(this);
        dlg.setInput(mGeoInput);
        if (dlg.exec() == QDialog::Accepted)
        {
            mGeoInput = dlg.inputParams();
            emit correctionRequested(mGeoInput);
        }
    });

    QAction* actCancel = createAction(tr("取消"), ":/icon/icon/delete.svg", "actGeoCancel");
    pnlExec->addSmallAction(actCancel);
    connect(actCancel, &QAction::triggered, this, [this]()
    {
        emit cancelCorrectionRequested();
    });

    pnlInput->setVisible(true);
    pnlMatch->setVisible(true);
    pnlExec->setVisible(true);
}

// ========================================================================
// Category 3: 图像融合
// ========================================================================
void MainWindow::createCategoryFusion(SARibbonCategory* page)
{
    // --- Panel 1: 输入影像 ---
    SARibbonPanel* pnlInput = page->addPanel(tr("输入影像"));

    QAction* actPan = createAction(tr("全色\n影像"), ":/icon/icon/save.svg", "actPan");
    pnlInput->addLargeAction(actPan);
    connect(actPan, &QAction::triggered, this, [this]()
    {
        QString file = QFileDialog::getOpenFileName(this, tr("选择全色影像"),
            QString(), tr("遥感影像 (*.tif *.tiff *.img);;所有文件 (*.*)"));
        Q_UNUSED(file);
    });

    QAction* actMs = createAction(tr("多光谱\n影像"), ":/icon/icon/Align-Left.svg", "actMs");
    pnlInput->addLargeAction(actMs);

    // --- Panel 2: 算法与参数 ---
    SARibbonPanel* pnlAlgo = page->addPanel(tr("算法与参数"));

    QComboBox* algoCombo = new QComboBox(this);
    algoCombo->addItems({ "IHS", "Brovey", "Gram-Schmidt", "PCA", "HPF", "Wavelet" });
    pnlAlgo->addSmallWidget(algoCombo);

    QAction* actPreview = createAction(tr("预览"), ":/icon/icon/undo.svg", "actPreview");
    pnlAlgo->addSmallAction(actPreview);

    QAction* actAdvFusion = createAction(tr("高级参数"), ":/icon/icon/layout.svg", "actAdvFusion");
    pnlAlgo->addSmallAction(actAdvFusion);
    connect(actAdvFusion, &QAction::triggered, this, [this]()
    {
        FusionDialog dlg(this);
        dlg.exec();
    });

    // --- Panel 3: 执行 ---
    SARibbonPanel* pnlExec = page->addPanel(tr("执行"));

    QAction* actFuse = createAction(tr("执行\n融合"), ":/icon/icon/folder-cog.svg", "actFuse");
    pnlExec->addLargeAction(actFuse);
    connect(actFuse, &QAction::triggered, this, [this, algoCombo]()
    {
        FusionDialog dlg(this);
        ImageFusionParams p;
        p.algorithm = algoCombo->currentText();
        dlg.setParams(p);
        if (dlg.exec() == QDialog::Accepted)
        {
            ImageFusionParams result = dlg.params();
            result.algorithm = algoCombo->currentText();
            emit fusionRequested(result);
        }
    });

    pnlInput->setVisible(true);
    pnlAlgo->setVisible(true);
    pnlExec->setVisible(true);
}

// ========================================================================
// Category 4: 镶嵌与成图
// ========================================================================
void MainWindow::createCategoryMosaic(SARibbonCategory* page)
{
    // --- Panel 1: 影像管理 ---
    SARibbonPanel* pnlImages = page->addPanel(tr("影像管理"));

    QAction* actAddImg = createAction(tr("添加\n影像"), ":/icon/icon/save.svg", "actAddImg");
    pnlImages->addLargeAction(actAddImg);
    connect(actAddImg, &QAction::triggered, this, [this]()
    {
        QStringList files = QFileDialog::getOpenFileNames(this, tr("选择待镶嵌影像"),
            QString(), tr("遥感影像 (*.tif *.tiff *.img);;所有文件 (*.*)"));
        if (!files.isEmpty())
            mMosaicImages.append(files);
    });

    QAction* actRemoveImg = createAction(tr("移除"), ":/icon/icon/delete.svg", "actRemoveImg");
    pnlImages->addSmallAction(actRemoveImg);
    connect(actRemoveImg, &QAction::triggered, this, [this]()
    {
        mMosaicImages.clear();
    });

    // --- Panel 2: 匀色与拼接线 ---
    SARibbonPanel* pnlParams = page->addPanel(tr("匀色与拼接线"));

    QComboBox* colorBalCombo = new QComboBox(this);
    colorBalCombo->addItems({ tr("无匀色"), tr("直方图匹配"), tr("Wallis滤波"), "LUT" });
    pnlParams->addSmallWidget(colorBalCombo);

    QComboBox* seamCombo = new QComboBox(this);
    seamCombo->addItems({ tr("无拼接线"), tr("Voronoi"), tr("最小成本路径") });
    pnlParams->addSmallWidget(seamCombo);

    QAction* actAdvMosaic = createAction(tr("高级参数"), ":/icon/icon/layout.svg", "actAdvMosaic");
    pnlParams->addSmallAction(actAdvMosaic);
    connect(actAdvMosaic, &QAction::triggered, this, [this, colorBalCombo, seamCombo]()
    {
        MosaicDialog dlg(this);
        MosaicParams initP;
        initP.inputImages = mMosaicImages;
        QString cb = colorBalCombo->currentText();
        if (cb.contains(tr("直方图"))) initP.colorBalanceMethod = QStringLiteral("HistogramMatching");
        else if (cb.contains(QStringLiteral("Wallis"))) initP.colorBalanceMethod = QStringLiteral("WallisFilter");
        else if (cb == QStringLiteral("LUT")) initP.colorBalanceMethod = QStringLiteral("LUT");

        QString sm = seamCombo->currentText();
        if (sm.contains(QStringLiteral("Voronoi"))) initP.seamlineMethod = QStringLiteral("Voronoi");
        else if (sm.contains(tr("最小成本"))) initP.seamlineMethod = QStringLiteral("MinCostPath");
        else initP.seamlineMethod = QStringLiteral("None");

        dlg.setParams(initP);
        dlg.exec();
    });

    // --- Panel 3: 执行 ---
    SARibbonPanel* pnlExec = page->addPanel(tr("执行"));

    QAction* actMosaic = createAction(tr("生成\n镶嵌"), ":/icon/icon/folder-cog.svg", "actMosaic");
    pnlExec->addLargeAction(actMosaic);
    connect(actMosaic, &QAction::triggered, this, [this, colorBalCombo, seamCombo]()
    {
        MosaicDialog dlg(this);
        // 将 Ribbon 面板的快速设置同步到对话框
        MosaicParams initP;
        initP.inputImages = mMosaicImages;
        QString cb = colorBalCombo->currentText();
        if (cb.contains(tr("直方图"))) initP.colorBalanceMethod = QStringLiteral("HistogramMatching");
        else if (cb.contains(QStringLiteral("Wallis"))) initP.colorBalanceMethod = QStringLiteral("WallisFilter");
        else if (cb == QStringLiteral("LUT")) initP.colorBalanceMethod = QStringLiteral("LUT");

        QString sm = seamCombo->currentText();
        if (sm.contains(QStringLiteral("Voronoi"))) initP.seamlineMethod = QStringLiteral("Voronoi");
        else if (sm.contains(tr("最小成本"))) initP.seamlineMethod = QStringLiteral("MinCostPath");
        else initP.seamlineMethod = QStringLiteral("None");

        dlg.setParams(initP);
        if (dlg.exec() == QDialog::Accepted)
            emit mosaicRequested(dlg.params());
    });

    QAction* actMosaicPreview = createAction(tr("预览"), ":/icon/icon/undo.svg", "actMosaicPreview");
    pnlExec->addSmallAction(actMosaicPreview);

    pnlImages->setVisible(true);
    pnlParams->setVisible(true);
    pnlExec->setVisible(true);
}

// ========================================================================
// Category 5: 可视化任务流设计器
// ========================================================================
void MainWindow::createCategoryWorkflow(SARibbonCategory* page)
{
    // --- Panel 1: 模板管理 ---
    SARibbonPanel* pnlTemplate = page->addPanel(tr("模板管理"));

    QAction* actNewWf = createAction(tr("新建\n工作流"), ":/icon/icon/save.svg", "actNewWf");
    pnlTemplate->addLargeAction(actNewWf);
    connect(actNewWf, &QAction::triggered, this, [this]()
    {
        QDialog* dlg = new QDialog(this);
        dlg->setWindowTitle(tr("工作流设计器"));
        dlg->resize(900, 600);
        auto* lay = new QVBoxLayout(dlg);
        WorkflowPanel* wfPanel = new WorkflowPanel(dlg);
        lay->addWidget(wfPanel);
        dlg->exec();
        delete dlg;
    });

    QAction* actOpenTmpl = createAction(tr("打开\n模板"), ":/icon/icon/file.svg", "actOpenTmpl");
    pnlTemplate->addLargeAction(actOpenTmpl);
    connect(actOpenTmpl, &QAction::triggered, this, [this]()
    {
        QString file = QFileDialog::getOpenFileName(this, tr("加载工作流模板"),
            QString(), tr("工作流模板 (*.wft *.xml *.json);;所有文件 (*.*)"));
        Q_UNUSED(file);
        // TODO: 加载模板后打开设计器
    });

    QAction* actSaveTmpl = createAction(tr("保存模板"), ":/icon/icon/save.svg", "actSaveTmpl");
    pnlTemplate->addSmallAction(actSaveTmpl);
    connect(actSaveTmpl, &QAction::triggered, this, [this]()
    {
        QString file = QFileDialog::getSaveFileName(this, tr("保存工作流模板"),
            QString(), tr("工作流模板 (*.wft *.xml *.json);;所有文件 (*.*)"));
        Q_UNUSED(file);
    });

    // --- Panel 2: 执行 ---
    SARibbonPanel* pnlExec = page->addPanel(tr("执行"));

    QAction* actRunWf = createAction(tr("运行\n工作流"), ":/icon/icon/folder-cog.svg", "actRunWf");
    pnlExec->addLargeAction(actRunWf);
    connect(actRunWf, &QAction::triggered, this, [this]()
    {
        // TODO: emit workflowRunRequested(nodeSequence);
    });

    QAction* actClearWf = createAction(tr("清空"), ":/icon/icon/delete.svg", "actClearWf");
    pnlExec->addSmallAction(actClearWf);

    pnlTemplate->setVisible(true);
    pnlExec->setVisible(true);
}

// ========================================================================
// Category 6: 批处理引擎
// ========================================================================
void MainWindow::createCategoryBatch(SARibbonCategory* page)
{
    // --- Panel 1: 任务管理 ---
    SARibbonPanel* pnlTask = page->addPanel(tr("任务管理"));

    QAction* actAddTask = createAction(tr("添加\n任务"), ":/icon/icon/save.svg", "actAddTask");
    pnlTask->addLargeAction(actAddTask);
    connect(actAddTask, &QAction::triggered, this, [this]()
    {
        QStringList files = QFileDialog::getOpenFileNames(this, tr("添加待处理影像"),
            QString(), tr("遥感影像 (*.tif *.tiff *.img);;所有文件 (*.*)"));
        Q_UNUSED(files);
        // TODO: 添加到批处理队列
    });

    QAction* actRmTask = createAction(tr("移除"), ":/icon/icon/delete.svg", "actRmTask");
    pnlTask->addSmallAction(actRmTask);

    QAction* actClearTasks = createAction(tr("清空队列"), ":/icon/icon/disable.svg", "actClearTasks");
    pnlTask->addSmallAction(actClearTasks);

    // --- Panel 2: 执行控制 ---
    SARibbonPanel* pnlControl = page->addPanel(tr("执行控制"));

    QAction* actStartAll = createAction(tr("开始\n全部"), ":/icon/icon/folder-cog.svg", "actStartAll");
    pnlControl->addLargeAction(actStartAll);
    connect(actStartAll, &QAction::triggered, this, [this]()
    {
        // TODO: emit startAllRequested();
    });

    QAction* actPause = createAction(tr("暂停"), ":/icon/icon/disable.svg", "actPause");
    pnlControl->addSmallAction(actPause);

    QAction* actResume = createAction(tr("继续"), ":/icon/icon/undo.svg", "actResume");
    pnlControl->addSmallAction(actResume);

    QAction* actCancel = createAction(tr("取消"), ":/icon/icon/delete.svg", "actCancel");
    pnlControl->addSmallAction(actCancel);

    QAction* actRetry = createAction(tr("重试"), ":/icon/icon/redo.svg", "actRetry");
    pnlControl->addSmallAction(actRetry);

    // --- Panel 3: 报告查看 ---
    SARibbonPanel* pnlReport = page->addPanel(tr("报告查看"));

    QAction* actReport = createAction(tr("导出\n报告"), ":/icon/icon/file.svg", "actReport");
    pnlReport->addLargeAction(actReport);
    connect(actReport, &QAction::triggered, this, [this]()
    {
        QString file = QFileDialog::getSaveFileName(this, tr("导出处理报告"),
            QString(), tr("HTML报告 (*.html);;文本 (*.txt);;JSON (*.json)"));
        Q_UNUSED(file);
        // TODO: emit reportExportRequested(file);
    });

    QAction* actViewDetail = createAction(tr("详细\n面板"), ":/icon/icon/layout.svg", "actViewDetail");
    pnlReport->addLargeAction(actViewDetail);
    connect(actViewDetail, &QAction::triggered, this, [this]()
    {
        QDialog* dlg = new QDialog(this);
        dlg->setWindowTitle(tr("批处理引擎 — 详细面板"));
        dlg->resize(800, 600);
        auto* lay = new QVBoxLayout(dlg);
        BatchProcessPanel* bpPanel = new BatchProcessPanel(dlg);
        lay->addWidget(bpPanel);
        dlg->exec();
        delete dlg;
    });

    pnlTask->setVisible(true);
    pnlControl->setVisible(true);
    pnlReport->setVisible(true);
}

// ========================================================================
// Quick Access Bar
// ========================================================================
void MainWindow::createQuickAccessBar()
{
    SARibbonQuickAccessBar* quickAccessBar = ribbonBar()->quickAccessBar();

    quickAccessBar->addAction(createAction("save", ":/icon/icon/save.svg", "save-quickbar"));
    quickAccessBar->addSeparator();

    QAction* actionUndo = createAction("undo", ":/icon/icon/undo.svg");
    actionUndo->setShortcut(QKeySequence("Ctrl+Shift+z"));
    actionUndo->setShortcutContext(Qt::ApplicationShortcut);
    quickAccessBar->addAction(actionUndo);
    connect(actionUndo, &QAction::triggered, this, &MainWindow::onUndoActionTriggered);

    QAction* actionRedo = createAction("redo", ":/icon/icon/redo.svg");
    actionRedo->setShortcut(QKeySequence("Ctrl+z"));
    actionRedo->setShortcutContext(Qt::ApplicationShortcut);
    quickAccessBar->addAction(actionRedo);
    connect(actionRedo, &QAction::triggered, this, &MainWindow::onRedoActionTriggered);

    quickAccessBar->addSeparator();

    mSearchEditor = new QLineEdit(this);
    mSearchEditor->setMinimumWidth(150);
    mSearchEditor->setPlaceholderText("Search");
    quickAccessBar->addWidget(mSearchEditor);
    connect(mSearchEditor, &QLineEdit::editingFinished, this, &MainWindow::onSearchEditorEditingFinished);
}

void MainWindow::createRightButtonGroup()
{
    SARibbonBar* currentRibbonBar = ribbonBar();
    if (!currentRibbonBar) return;
    SARibbonButtonGroupWidget* rightBar = currentRibbonBar->rightButtonGroup();
    QAction* actionHelp = createAction(tr("help"), ":/icon/icon/help.svg");
    connect(actionHelp, &QAction::triggered, this, &MainWindow::onActionHelpTriggered);
    rightBar->addAction(actionHelp);
}

void MainWindow::createWindowButtonGroupBar()
{
    SARibbonSystemButtonBar* windowButtonBar = this->windowButtonBar();
    if (!windowButtonBar) return;
    QAction* actionLogin = new QAction(QIcon(), tr("Login"), this);
    QAction* actionHelp  = createAction(tr("help"), ":/icon/icon/help.svg");
    connect(actionLogin, &QAction::triggered, this, &MainWindow::onLoginActionTriggered);
    connect(actionHelp, &QAction::triggered, this, &MainWindow::onActionHelpTriggered);
    windowButtonBar->addAction(actionLogin);
    windowButtonBar->addAction(actionHelp);
}

void MainWindow::closeEvent(QCloseEvent* closeEvent)
{
    auto userResponse = QMessageBox::question(this, tr("question"), tr("Confirm whether to exit"));
    if (userResponse == QMessageBox::Yes)
    {
        closeEvent->accept();
    } else
    {
        closeEvent->ignore();
    }
}

// ========== Slot implementations ==========

void MainWindow::onActionHelpTriggered()
{
    QMessageBox::information(
        this,
        tr("infomation"),
        tr("\n ==============="
           "\n 多源遥感影像全流程批处理系统 v0.1"
           "\n 功能模块：辐射定标 | 几何校正 | 图像融合 | 镶嵌成图"
           "\n 可选模块：工作流设计器 | 批处理引擎"
           "\n 基于 SARibbonBar v%1"
           "\n ===============")
            .arg(SARibbonBar::versionString())
    );
}

void MainWindow::onUndoActionTriggered() { /* TODO */ }
void MainWindow::onRedoActionTriggered() { /* TODO */ }
void MainWindow::onSearchEditorEditingFinished() { /* TODO */ }
void MainWindow::onLoginActionTriggered() { /* TODO */ }

// ========== Action factory methods ==========

QAction* MainWindow::createAction(const QString& text, const QString& iconPath, const QString& objName)
{
    QAction* newAction = new QAction(this);
    newAction->setText(text);
    newAction->setIcon(QIcon(iconPath));
    newAction->setObjectName(objName);
    return newAction;
}

QAction* MainWindow::createAction(const QString& text, const QString& iconPath)
{
    QAction* newAction = new QAction(this);
    newAction->setText(text);
    newAction->setIcon(QIcon(iconPath));
    newAction->setObjectName(text);
    return newAction;
}
