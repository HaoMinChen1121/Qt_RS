#include "PipelineDialog.h"
#include "controllers/PipelineExpander.h"
#include "dataaccess/SensorProductFactory.h"
#include "ui/RadiometricDialog.h"
#include "ui/GeometricDialog.h"
#include "ui/FusionDialog.h"
#include "ui/MosaicDialog.h"

#include <QListWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QIntValidator>
#include <QDebug>

PipelineDialog::PipelineDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("一键全流程处理"));
    resize(820, 620);
    setMinimumSize(640, 480);

    // 默认使用标准流水线
    mProject.pipeline = PipelineExpander::standardPipeline();
    setupUI();
    populateStages(mProject.pipeline);
}

void PipelineDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // ── 工程名称 ──
    auto* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel(QStringLiteral("工程名称:"), this));
    mProjectName = new QLineEdit(this);
    mProjectName->setPlaceholderText(QStringLiteral("输入工程名称，如: 某地区2024年遥感制图"));
    nameLayout->addWidget(mProjectName);
    mainLayout->addLayout(nameLayout);

    // ── 上部分割器: 文件列表 | 属性 + 阶段 ──
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // ─── 左侧: 影像文件列表 ───
    auto* leftWidget = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    auto* fileGroup = new QGroupBox(QStringLiteral("输入影像"), leftWidget);
    auto* fileGroupLayout = new QVBoxLayout(fileGroup);

    mFileList = new QListWidget(fileGroup);
    mFileList->setMinimumWidth(200);
    mFileList->setAlternatingRowColors(true);
    fileGroupLayout->addWidget(mFileList);

    auto* fileBtnLayout = new QHBoxLayout();
    mAddBtn = new QPushButton(QStringLiteral("添加影像..."), fileGroup);
    mRemoveBtn = new QPushButton(QStringLiteral("移除"), fileGroup);
    fileBtnLayout->addWidget(mAddBtn);
    fileBtnLayout->addWidget(mRemoveBtn);
    fileBtnLayout->addStretch();
    fileGroupLayout->addLayout(fileBtnLayout);

    leftLayout->addWidget(fileGroup);
    splitter->addWidget(leftWidget);

    // ─── 右侧: 属性 + 阶段 ───
    auto* rightWidget = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    // 影像属性
    auto* propGroup = new QGroupBox(QStringLiteral("影像属性"), rightWidget);
    auto* propForm = new QFormLayout(propGroup);

    mRoleCombo = new QComboBox(propGroup);
    mRoleCombo->addItem(QStringLiteral("镶嵌输入"), static_cast<int>(ImageRole::MosaicInput));
    mRoleCombo->addItem(QStringLiteral("全色影像"), static_cast<int>(ImageRole::Panchromatic));
    mRoleCombo->addItem(QStringLiteral("几何参考"), static_cast<int>(ImageRole::Reference));
    mRoleCombo->addItem(QStringLiteral("匀色参考"), static_cast<int>(ImageRole::HistReference));
    propForm->addRow(QStringLiteral("角色:"), mRoleCombo);

    mSensorLabel = new QLabel(QStringLiteral("(自动检测)"), propGroup);
    propForm->addRow(QStringLiteral("传感器:"), mSensorLabel);

    // 波段选择 — 四列: 红 / 绿 / 蓝 / 全色
    auto* bandWidget = new QWidget(propGroup);
    auto* bandLayout = new QHBoxLayout(bandWidget);
    bandLayout->setContentsMargins(0, 0, 0, 0);
    bandLayout->setSpacing(8);

    auto makeBandEdit = [&](const QString& label, const QString& tooltip) -> QLineEdit* {
        auto* w = new QWidget(bandWidget);
        auto* l = new QVBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(4);
        auto* lbl = new QLabel(label, w);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QStringLiteral("font-size: 11px; padding: 0 4px;"));
        auto* edit = new QLineEdit(w);
        edit->setFixedWidth(60);
        edit->setAlignment(Qt::AlignCenter);
        edit->setPlaceholderText(QStringLiteral("波段号"));
        edit->setToolTip(tooltip);
        l->addWidget(lbl);
        l->addWidget(edit);
        bandLayout->addWidget(w);
        return edit;
    };

    mBandR   = makeBandEdit(QStringLiteral("红波段"),
        QStringLiteral("Red 通道对应的传感器物理波段编号 (如 Landsat-8 B4)"));
    mBandG   = makeBandEdit(QStringLiteral("绿波段"),
        QStringLiteral("Green 通道对应的传感器物理波段编号 (如 Landsat-8 B3)"));
    mBandB   = makeBandEdit(QStringLiteral("蓝波段"),
        QStringLiteral("Blue 通道对应的传感器物理波段编号 (如 Landsat-8 B2)"));
    mBandPan = makeBandEdit(QStringLiteral("全色波段"),
        QStringLiteral("全色高分辨率波段编号 (如 Landsat-8 B8), 无则留空"));

    bandLayout->addStretch();

    propForm->addRow(QStringLiteral("波段选择:"), bandWidget);
    rightLayout->addWidget(propGroup);

    // 处理阶段
    auto* stageGroup = new QGroupBox(QStringLiteral("处理阶段"), rightWidget);
    mStageWidgets.clear();  // will be populated by populateStages after setupUI
    // 阶段复选框动态添加到 stageLayout
    auto* stageLayout = new QVBoxLayout(stageGroup);
    stageLayout->setObjectName(QStringLiteral("stageLayout"));
    rightLayout->addWidget(stageGroup);

    splitter->addWidget(rightWidget);
    splitter->setSizes({260, 400});
    mainLayout->addWidget(splitter);

    // ── 输出 ──
    auto* outLayout = new QHBoxLayout();
    outLayout->addWidget(new QLabel(QStringLiteral("输出目录:"), this));
    mOutputDir = new QLineEdit(this);
    mOutputDir->setPlaceholderText(QStringLiteral("选择输出目录..."));
    outLayout->addWidget(mOutputDir);
    mOutputBrowseBtn = new QPushButton(QStringLiteral("浏览..."), this);
    outLayout->addWidget(mOutputBrowseBtn);
    mainLayout->addLayout(outLayout);

    mCleanupCheck = new QCheckBox(QStringLiteral("完成后清理中间产物"), this);
    mCleanupCheck->setChecked(false);
    mainLayout->addWidget(mCleanupCheck);

    mAutoConfirmCheck = new QCheckBox(QStringLiteral("自动确认处理结果（不弹窗）"), this);
    mAutoConfirmCheck->setChecked(true);
    mAutoConfirmCheck->setToolTip(QStringLiteral("勾选后各阶段处理完成不弹出确认对话框，直接继续"));
    mainLayout->addWidget(mAutoConfirmCheck);

    // ── 底部操作栏 ──
    auto* bottomLayout = new QHBoxLayout();

    mSaveBtn = new QPushButton(QStringLiteral("保存工程..."), this);
    mLoadBtn = new QPushButton(QStringLiteral("加载工程..."), this);
    mRunBtn  = new QPushButton(QStringLiteral("▶ 开始处理"), this);
    mRunBtn->setStyleSheet(QStringLiteral(
        "QPushButton { font-weight: bold; min-height: 32px; "
        "background-color: #0078d4; color: white; border-radius: 4px; }"
        "QPushButton:hover { background-color: #106ebe; }"));

    bottomLayout->addWidget(mSaveBtn);
    bottomLayout->addWidget(mLoadBtn);
    bottomLayout->addStretch();
    bottomLayout->addWidget(mRunBtn);
    mainLayout->addLayout(bottomLayout);

    // 进度
    mProgressBar = new QProgressBar(this);
    mProgressBar->setVisible(false);
    mainLayout->addWidget(mProgressBar);

    mStatusLabel = new QLabel(QStringLiteral("就绪 - 添加影像文件后点击 [开始处理]"), this);
    mStatusLabel->setAlignment(Qt::AlignCenter);
    mStatusLabel->setStyleSheet(QStringLiteral("color: gray;"));
    mainLayout->addWidget(mStatusLabel);

    // ── 信号连接 ──
    connect(mAddBtn,  &QPushButton::clicked, this, &PipelineDialog::onAddFiles);
    connect(mRemoveBtn, &QPushButton::clicked, this, &PipelineDialog::onRemoveFile);
    connect(mFileList, &QListWidget::currentRowChanged, this, &PipelineDialog::onFileSelectionChanged);
    connect(mRoleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PipelineDialog::onRoleChanged);
    connect(mOutputBrowseBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"));
        if (!dir.isEmpty()) mOutputDir->setText(dir);
    });
    connect(mSaveBtn, &QPushButton::clicked, this, &PipelineDialog::onSaveProject);
    connect(mLoadBtn, &QPushButton::clicked, this, &PipelineDialog::onLoadProject);
    connect(mRunBtn,  &QPushButton::clicked, this, &PipelineDialog::onRun);
}

void PipelineDialog::populateStages(const PipelineDefinition& pipeline)
{
    // 找到 stageLayout 并清空已有控件
    auto* stageGroup = findChild<QGroupBox*>();
    QVBoxLayout* stageLayout = nullptr;
    for (auto* gb : findChildren<QGroupBox*>())
    {
        if (gb->title() == QStringLiteral("处理阶段"))
        {
            stageLayout = qobject_cast<QVBoxLayout*>(gb->layout());
            break;
        }
    }
    if (!stageLayout)
    {
        // fallback: find by objectName
        stageLayout = findChild<QVBoxLayout*>(QStringLiteral("stageLayout"));
    }
    if (!stageLayout) return;

    // 清空
    mStageWidgets.clear();
    while (stageLayout->count() > 0)
    {
        QLayoutItem* item = stageLayout->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    for (int i = 0; i < pipeline.stages.size(); ++i)
    {
        const auto& stage = pipeline.stages[i];

        auto* row = new QWidget();
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);

        auto* cb = new QCheckBox(stage.displayName, row);
        cb->setChecked(stage.enabled);
        if (stage.required)
        {
            cb->setEnabled(false);
            cb->setChecked(true);
        }
        rowLayout->addWidget(cb);

        // 配置按钮: 无参数或必须阶段隐藏
        auto* cfgBtn = new QPushButton(QStringLiteral("..."), row);
        cfgBtn->setFixedWidth(28);
        cfgBtn->setToolTip(QStringLiteral("配置 %1 参数").arg(stage.displayName));
        bool hideCfg = stage.required || stage.params.isEmpty()
                       || stage.stageId == QStringLiteral("Composite");
        cfgBtn->setVisible(!hideCfg);
        rowLayout->addWidget(cfgBtn);
        rowLayout->addStretch();

        stageLayout->addWidget(row);
        int idx = static_cast<int>(mStageWidgets.size());
        mStageWidgets.append({cb, cfgBtn, stage});
        connect(cfgBtn, &QPushButton::clicked, this, [this, idx]() {
            onStageConfig(idx);
        });

        // 融合复选框联动全色波段字段
        if (stage.stageId == QStringLiteral("Fusion"))
        {
            connect(cb, &QCheckBox::toggled, this, &PipelineDialog::updateBandFieldStates);
        }
    }
}

// ── 辅助: QVariantMap → RadiometricCorrectionParams ──
static RadiometricCorrectionParams mapToRadiometric(const QMap<QString, QVariant>& m)
{
    RadiometricCorrectionParams p;
    p.calibrationType  = m.value(QStringLiteral("calibrationType"), QStringLiteral("DN2Radiance")).toString();
    p.outputDataType   = m.value(QStringLiteral("outputDataType"), QStringLiteral("Float32")).toString();
    p.autoGainOffset   = m.value(QStringLiteral("autoGainOffset"), true).toBool();
    p.manualGain       = m.value(QStringLiteral("manualGain"), 1.0).toDouble();
    p.manualOffset     = m.value(QStringLiteral("manualOffset"), 0.0).toDouble();
    p.atmModel         = m.value(QStringLiteral("atmModel"), QStringLiteral("6S")).toString();
    p.aerosolModel     = m.value(QStringLiteral("aerosolModel"), QStringLiteral("Continental")).toString();
    p.atmosphericModel = m.value(QStringLiteral("atmosphericModel"), QStringLiteral("MidLatSummer")).toString();
    p.aot550           = m.value(QStringLiteral("aot550"), 0.2).toDouble();
    p.waterVapor       = m.value(QStringLiteral("waterVapor"), 2.0).toDouble();
    p.ozone            = m.value(QStringLiteral("ozone"), 0.3).toDouble();
    p.sensorType         = m.value(QStringLiteral("sensorType")).toString();
    p.outputFormat       = m.value(QStringLiteral("outputFormat"), QStringLiteral("GeoTIFF")).toString();
    p.solarZenithAngle   = m.value(QStringLiteral("solarZenithAngle"), 0.0).toDouble();
    p.solarAzimuthAngle  = m.value(QStringLiteral("solarAzimuthAngle"), 0.0).toDouble();
    p.sensorZenithAngle  = m.value(QStringLiteral("sensorZenithAngle"), 0.0).toDouble();
    p.sensorAzimuthAngle = m.value(QStringLiteral("sensorAzimuthAngle"), 0.0).toDouble();
    p.earthSunDistance   = m.value(QStringLiteral("earthSunDistance"), 1.0).toDouble();
    p.targetElevation    = m.value(QStringLiteral("targetElevation"), 0.0).toDouble();
    p.sensorAltitude     = m.value(QStringLiteral("sensorAltitude"), 800.0).toDouble();
    return p;
}

static QMap<QString, QVariant> radiometricToMap(const RadiometricCorrectionParams& p)
{
    QMap<QString, QVariant> m;
    m[QStringLiteral("calibrationType")]   = p.calibrationType;
    m[QStringLiteral("outputDataType")]    = p.outputDataType;
    m[QStringLiteral("autoGainOffset")]    = p.autoGainOffset;
    m[QStringLiteral("manualGain")]        = p.manualGain;
    m[QStringLiteral("manualOffset")]      = p.manualOffset;
    m[QStringLiteral("atmModel")]          = p.atmModel;
    m[QStringLiteral("aerosolModel")]      = p.aerosolModel;
    m[QStringLiteral("atmosphericModel")]  = p.atmosphericModel;
    m[QStringLiteral("aot550")]            = p.aot550;
    m[QStringLiteral("waterVapor")]        = p.waterVapor;
    m[QStringLiteral("ozone")]             = p.ozone;
    m[QStringLiteral("sensorType")]        = p.sensorType;
    m[QStringLiteral("outputFormat")]      = p.outputFormat;
    m[QStringLiteral("solarZenithAngle")]  = p.solarZenithAngle;
    m[QStringLiteral("solarAzimuthAngle")] = p.solarAzimuthAngle;
    m[QStringLiteral("sensorZenithAngle")] = p.sensorZenithAngle;
    m[QStringLiteral("sensorAzimuthAngle")]= p.sensorAzimuthAngle;
    m[QStringLiteral("earthSunDistance")]   = p.earthSunDistance;
    m[QStringLiteral("targetElevation")]    = p.targetElevation;
    m[QStringLiteral("sensorAltitude")]     = p.sensorAltitude;
    return m;
}

// ── 辅助: QVariantMap → GeometricInput (GeometricDialog 使用) ──
static GeometricInput mapToGeometric(const QMap<QString, QVariant>& m)
{
    GeometricInput g;
    g.modelType      = m.value(QStringLiteral("modelType"), QStringLiteral("Polynomial2")).toString();
    g.resampleMethod = m.value(QStringLiteral("resampleMethod"), QStringLiteral("Bilinear")).toString();
    g.outputProjection = m.value(QStringLiteral("outputProjection")).toString();
    g.outputPixelSizeX = m.value(QStringLiteral("outputPixelSizeX"), 0.0).toDouble();
    g.outputPixelSizeY = m.value(QStringLiteral("outputPixelSizeY"), 0.0).toDouble();
    return g;
}

static QMap<QString, QVariant> geometricToMap(const GeometricInput& g)
{
    QMap<QString, QVariant> m;
    m[QStringLiteral("modelType")]        = g.modelType;
    m[QStringLiteral("resampleMethod")]   = g.resampleMethod;
    m[QStringLiteral("outputProjection")] = g.outputProjection;
    m[QStringLiteral("outputPixelSizeX")] = g.outputPixelSizeX;
    m[QStringLiteral("outputPixelSizeY")] = g.outputPixelSizeY;
    return m;
}

// ── 辅助: QVariantMap → ImageFusionParams ──
static ImageFusionParams mapToFusion(const QMap<QString, QVariant>& m)
{
    ImageFusionParams p;
    p.algorithm = m.value(QStringLiteral("algorithm"), QStringLiteral("GramSchmidt")).toString();
    p.ihsColorModel = m.value(QStringLiteral("ihsColorModel"), QStringLiteral("HSI")).toString();
    p.gsSimulationMethod = m.value(QStringLiteral("gsSimulationMethod"), QStringLiteral("Average")).toString();
    p.hpfKernelSize = m.value(QStringLiteral("hpfKernelSize"), 5).toInt();
    p.hpfWeight     = m.value(QStringLiteral("hpfWeight"), 0.5).toDouble();
    p.waveletDecompositionLevel = m.value(QStringLiteral("waveletDecompositionLevel"), 3).toInt();
    p.waveletType   = m.value(QStringLiteral("waveletType"), QStringLiteral("Daubechies4")).toString();
    return p;
}

static QMap<QString, QVariant> fusionToMap(const ImageFusionParams& p)
{
    QMap<QString, QVariant> m;
    m[QStringLiteral("algorithm")] = p.algorithm;
    m[QStringLiteral("ihsColorModel")] = p.ihsColorModel;
    m[QStringLiteral("gsSimulationMethod")] = p.gsSimulationMethod;
    m[QStringLiteral("hpfKernelSize")] = p.hpfKernelSize;
    m[QStringLiteral("hpfWeight")] = p.hpfWeight;
    m[QStringLiteral("waveletDecompositionLevel")] = p.waveletDecompositionLevel;
    m[QStringLiteral("waveletType")] = p.waveletType;
    return m;
}

// ── 辅助: QVariantMap → MosaicParams ──
static MosaicParams mapToMosaic(const QMap<QString, QVariant>& m)
{
    MosaicParams p;
    p.colorBalanceMethod = m.value(QStringLiteral("colorBalanceMethod"), QStringLiteral("HistogramMatching")).toString();
    p.seamlineMethod     = m.value(QStringLiteral("seamlineMethod"), QStringLiteral("Voronoi")).toString();
    p.featheringWidth    = m.value(QStringLiteral("featheringWidth"), 10).toInt();
    p.featheringType     = m.value(QStringLiteral("featheringType"), QStringLiteral("Linear")).toString();
    p.outputFormat       = m.value(QStringLiteral("outputFormat"), QStringLiteral("GeoTIFF")).toString();
    p.backgroundValue    = m.value(QStringLiteral("backgroundValue"), 0).toInt();
    p.blockSize          = m.value(QStringLiteral("blockSize"), 512).toInt();
    return p;
}

static QMap<QString, QVariant> mosaicToMap(const MosaicParams& p)
{
    QMap<QString, QVariant> m;
    m[QStringLiteral("colorBalanceMethod")] = p.colorBalanceMethod;
    m[QStringLiteral("seamlineMethod")]     = p.seamlineMethod;
    m[QStringLiteral("featheringWidth")]    = p.featheringWidth;
    m[QStringLiteral("featheringType")]     = p.featheringType;
    m[QStringLiteral("outputFormat")]       = p.outputFormat;
    m[QStringLiteral("backgroundValue")]    = p.backgroundValue;
    m[QStringLiteral("blockSize")]          = p.blockSize;
    return m;
}

// ────────────────────────────────────────────────────────────
void PipelineDialog::onStageConfig(int idx)
{
    if (idx < 0 || idx >= mStageWidgets.size()) return;

    StageWidget& sw = mStageWidgets[idx];
    const QString& stageId = sw.stage.stageId;

    if (stageId == QStringLiteral("Radiometric") || stageId == QStringLiteral("Calibrate") || stageId == QStringLiteral("AtmCorrect"))
    {
        auto rp = mapToRadiometric(sw.stage.params);
        SensorInfo sensorInfo;  // 产品的元数据快照

        // 从工程影像清单预填输入文件并提取元数据
        for (const auto& src : mProject.imageSources)
        {
            if (src.role == ImageRole::MosaicInput)
            {
                rp.inputFiles.append(src.filePath);

                // 打开第一个产品获取 SensorInfo（波段/增益/采集时间等）
                if (sensorInfo.bands.isEmpty())
                {
                    QScopedPointer<ISensorProduct> prod(createSensorProduct(src.filePath));
                    if (prod && prod->open(src.filePath))
                    {
                        sensorInfo = prod->sensorInfo();
                        // 交叉填入波段选择对应的增益/偏置
                        for (const auto& bs : src.bandSelections)
                        {
                            for (int bn : bs.bandNumbers)
                            {
                                double gain, offset;
                                if (sensorInfo.bandCalibration(bn, gain, offset))
                                {
                                    rp.manualGain   = gain;
                                    rp.manualOffset = offset;
                                }
                            }
                        }
                        prod->close();
                    }
                }
            }
        }

        // 自动增益时填入传感器默认值
        if (rp.autoGainOffset && !sensorInfo.bands.isEmpty())
        {
            rp.solarZenithAngle  = sensorInfo.solarZenithAngle;
            rp.solarAzimuthAngle = sensorInfo.solarAzimuthAngle;
            rp.sensorZenithAngle = sensorInfo.sensorZenithAngle;
            rp.sensorAzimuthAngle = sensorInfo.sensorAzimuthAngle;
            rp.earthSunDistance  = sensorInfo.earthSunDistance;
            rp.sensorType        = sensorInfo.sensorType;
        }

        RadiometricDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("配置 — %1").arg(sw.stage.displayName));
        dlg.setParams(rp);
        if (!sensorInfo.bands.isEmpty())
            dlg.setSensorInfo(sensorInfo);
        if (dlg.exec() == QDialog::Accepted)
        {
            bool savedDoAtm = sw.stage.params.value(
                QStringLiteral("doAtmosphericCorrection"), true).toBool();
            sw.stage.params = radiometricToMap(dlg.params());
            sw.stage.params[QStringLiteral("doAtmosphericCorrection")] = savedDoAtm;
        }
    }
    else if (stageId == QStringLiteral("Geometric") || stageId == QStringLiteral("Clip") || stageId == QStringLiteral("Resample"))
    {
        GeometricDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("配置 — %1").arg(sw.stage.displayName));
        dlg.setInput(mapToGeometric(sw.stage.params));
        if (dlg.exec() == QDialog::Accepted)
            sw.stage.params = geometricToMap(dlg.inputParams());
    }
    else if (stageId == QStringLiteral("Fusion"))
    {
        FusionDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("配置 — %1").arg(sw.stage.displayName));
        dlg.setParams(mapToFusion(sw.stage.params));
        if (dlg.exec() == QDialog::Accepted)
            sw.stage.params = fusionToMap(dlg.params());
    }
    else if (stageId == QStringLiteral("Mosaic"))
    {
        MosaicDialog dlg(this);
        dlg.setWindowTitle(QStringLiteral("配置 — %1").arg(sw.stage.displayName));
        dlg.setParams(mapToMosaic(sw.stage.params));
        if (dlg.exec() == QDialog::Accepted)
            sw.stage.params = mosaicToMap(dlg.params());
    }
    else
    {
        // Read / Write / 未知阶段 — 无参数对话框
        return;
    }
}

// ────────────────────────────────────────────────────────────
// 文件操作
// ────────────────────────────────────────────────────────────
void PipelineDialog::onAddFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(this,
        QStringLiteral("选择遥感影像"),
        QString(),
        QStringLiteral("遥感影像 (*.tif *.tiff *.img *.zip *.SAFE *.tar.gz);;所有文件 (*.*)"));

    for (const QString& f : files)
    {
        QFileInfo fi(f);
        ImageSource src;
        src.sourceId    = QStringLiteral("src_%1").arg(mProject.imageSources.size());
        src.displayName = fi.completeBaseName();
        src.filePath    = f;
        src.role        = ImageRole::MosaicInput;
        // 用 SensorProductFactory 真正检测传感器类型
        QScopedPointer<ISensorProduct> prod(createSensorProduct(f));
        if (prod && prod->open(f))
        {
            src.sensorType = prod->sensorType();
            prod->close();
        }

        mProject.imageSources.append(src);

        QString roleText = (src.role == ImageRole::MosaicInput)
                           ? QStringLiteral("镶嵌输入") : QStringLiteral("其他");
        mFileList->addItem(QStringLiteral("[%1] %2 — %3")
                           .arg(src.sensorType.isEmpty() ? QStringLiteral("?") : src.sensorType,
                                src.displayName, roleText));
    }

    if (!files.isEmpty())
        mFileList->setCurrentRow(mFileList->count() - 1);
}

void PipelineDialog::onRemoveFile()
{
    int row = mFileList->currentRow();
    if (row < 0 || row >= mProject.imageSources.size()) return;

    delete mFileList->takeItem(row);
    mProject.imageSources.removeAt(row);
}

void PipelineDialog::onFileSelectionChanged()
{
    int row = mFileList->currentRow();
    if (row < 0 || row >= mProject.imageSources.size()) return;
    updateFileInfo(row);
}

void PipelineDialog::updateFileInfo(int row)
{
    if (row < 0 || row >= mProject.imageSources.size()) return;
    const ImageSource& src = mProject.imageSources[row];

    // 角色
    int roleIdx = mRoleCombo->findData(static_cast<int>(src.role));
    if (roleIdx >= 0) mRoleCombo->setCurrentIndex(roleIdx);

    // 传感器
    mSensorLabel->setText(src.sensorType.isEmpty()
                          ? QStringLiteral("(未能识别)")
                          : src.sensorType);

    // 波段
    auto findBandByPurpose = [&](const QString& purpose) -> QString {
        for (const auto& bs : src.bandSelections)
        {
            if (bs.purpose == purpose && !bs.bandNumbers.isEmpty())
                return QString::number(bs.bandNumbers.first());
        }
        return {};
    };

    // 默认查找 "Multispectral" 的 RGB
    for (const auto& bs : src.bandSelections)
    {
        if (bs.purpose == QStringLiteral("Multispectral"))
        {
            if (bs.bandNumbers.size() >= 3)
            {
                mBandR->setText(QString::number(bs.bandNumbers[0]));
                mBandG->setText(QString::number(bs.bandNumbers[1]));
                mBandB->setText(QString::number(bs.bandNumbers[2]));
            }
        }
        if (bs.purpose == QStringLiteral("Panchromatic") && !bs.bandNumbers.isEmpty())
        {
            mBandPan->setText(QString::number(bs.bandNumbers.first()));
        }
    }

    updateBandFieldStates();
}

void PipelineDialog::onRoleChanged(int index)
{
    int row = mFileList->currentRow();
    if (row < 0 || row >= mProject.imageSources.size()) return;

    auto role = static_cast<ImageRole>(mRoleCombo->itemData(index).toInt());
    mProject.imageSources[row].role = role;

    // 更新列表显示
    QString roleText;
    switch (role)
    {
    case ImageRole::MosaicInput:  roleText = QStringLiteral("镶嵌输入"); break;
    case ImageRole::Panchromatic: roleText = QStringLiteral("全色影像"); break;
    case ImageRole::Reference:    roleText = QStringLiteral("几何参考"); break;
    case ImageRole::HistReference:roleText = QStringLiteral("匀色参考"); break;
    }
    const ImageSource& src = mProject.imageSources[row];
    mFileList->item(row)->setText(QStringLiteral("[%1] %2 — %3")
        .arg(src.sensorType.isEmpty() ? QStringLiteral("?") : src.sensorType,
             src.displayName, roleText));

    updateBandFieldStates();
}

void PipelineDialog::updateBandFieldStates()
{
    int row = mFileList->currentRow();
    if (row < 0 || row >= mProject.imageSources.size()) return;
    const ImageSource& src = mProject.imageSources[row];

    // 全色波段字段是否可编辑: 需当前影像角色为全色 或 融合阶段已启用
    bool fusionEnabled = false;
    for (const auto& sw : mStageWidgets)
    {
        if (sw.stage.stageId == QStringLiteral("Fusion") && sw.checkbox->isChecked())
        {
            fusionEnabled = true;
            break;
        }
    }
    bool hasPanRole = (src.role == ImageRole::Panchromatic);
    mBandPan->setEnabled(hasPanRole || fusionEnabled);

    // RGB 字段在全色影像角色下无意义
    mBandR->setEnabled(src.role != ImageRole::Panchromatic);
    mBandG->setEnabled(src.role != ImageRole::Panchromatic);
    mBandB->setEnabled(src.role != ImageRole::Panchromatic);
}

// ────────────────────────────────────────────────────────────
// 工程持久化
// ────────────────────────────────────────────────────────────
void PipelineDialog::onSaveProject()
{
    syncFromUI();
    QString file = QFileDialog::getSaveFileName(this,
        QStringLiteral("保存工程"), QString(),
        QStringLiteral("遥感工程文件 (*.rjp);;所有文件 (*.*)"));
    if (!file.isEmpty())
        emit saveRequested(file);
}

void PipelineDialog::onLoadProject()
{
    QString file = QFileDialog::getOpenFileName(this,
        QStringLiteral("加载工程"), QString(),
        QStringLiteral("遥感工程文件 (*.rjp);;所有文件 (*.*)"));
    if (!file.isEmpty())
        emit loadRequested(file);
}

// ────────────────────────────────────────────────────────────
// 执行
// ────────────────────────────────────────────────────────────
void PipelineDialog::onRun()
{
    syncFromUI();

    if (!mProject.isValid())
    {
        QMessageBox::warning(this, QStringLiteral("工程不完整"),
            QStringLiteral("请确保: \n"
                           "1) 已填写工程名称\n"
                           "2) 已添加至少一幅影像\n"
                           "3) 已选择输出目录"));
        return;
    }

    mProgressBar->setVisible(true);
    mProgressBar->setValue(0);
    mRunBtn->setEnabled(false);
    mStatusLabel->setText(QStringLiteral("处理中..."));
    mStatusLabel->setStyleSheet(QStringLiteral("color: orange; font-weight: bold;"));

    emit runRequested(mProject);
}

// ────────────────────────────────────────────────────────────
// 进度回调
// ────────────────────────────────────────────────────────────
void PipelineDialog::onStageProgress(const QString& stageId, int percent,
                                      const QString& status)
{
    mProgressBar->setValue(percent);
    mStatusLabel->setText(QStringLiteral("[%1] %2").arg(stageId, status));
    mStatusLabel->setStyleSheet(QStringLiteral("color: orange;"));
}

void PipelineDialog::onPipelineFinished(bool success, const QString& outputPath)
{
    mProgressBar->setVisible(false);
    mRunBtn->setEnabled(true);

    if (success)
    {
        mStatusLabel->setText(QStringLiteral("处理完成: %1").arg(outputPath));
        mStatusLabel->setStyleSheet(QStringLiteral("color: green; font-weight: bold;"));
    }
    else
    {
        mStatusLabel->setText(QStringLiteral("处理失败"));
        mStatusLabel->setStyleSheet(QStringLiteral("color: red; font-weight: bold;"));
    }
}

void PipelineDialog::onPipelineError(const QString& nodeId, const QString& error)
{
    mStatusLabel->setText(QStringLiteral("错误 [%1]: %2").arg(nodeId, error));
    mStatusLabel->setStyleSheet(QStringLiteral("color: red;"));
}

// ────────────────────────────────────────────────────────────
// 双向同步
// ────────────────────────────────────────────────────────────
void PipelineDialog::setProject(const Project& project)
{
    mProject = project;

    // 工程名称
    mProjectName->setText(mProject.projectName);

    // 重建文件列表
    mFileList->clear();
    for (const auto& src : mProject.imageSources)
    {
        QString roleText;
        switch (src.role)
        {
        case ImageRole::MosaicInput:  roleText = QStringLiteral("镶嵌输入"); break;
        case ImageRole::Panchromatic: roleText = QStringLiteral("全色影像"); break;
        case ImageRole::Reference:    roleText = QStringLiteral("几何参考"); break;
        case ImageRole::HistReference:roleText = QStringLiteral("匀色参考"); break;
        }
        mFileList->addItem(QStringLiteral("[%1] %2 — %3")
            .arg(src.sensorType.isEmpty() ? QStringLiteral("?") : src.sensorType,
                 src.displayName, roleText));
    }

    // 重建阶段复选框
    populateStages(mProject.pipeline);

    // 输出路径
    mOutputDir->setText(mProject.output.outputDirectory);
    mCleanupCheck->setChecked(mProject.output.cleanupIntermediates);
    mAutoConfirmCheck->setChecked(mProject.output.autoConfirm);
    if (!mProject.imageSources.isEmpty())
        mFileList->setCurrentRow(0);
}

Project PipelineDialog::project() const
{
    // 注意: 返回副本，调用者应使用 syncFromUI 后再取
    return mProject;
}

void PipelineDialog::syncFromUI()
{
    // 同步工程名称
    mProject.projectName = mProjectName->text().trimmed();

    // 同步阶段启用状态和参数
    for (int i = 0; i < mStageWidgets.size() && i < mProject.pipeline.stages.size(); ++i)
    {
        mProject.pipeline.stages[i].enabled = mStageWidgets[i].checkbox->isChecked();
        mProject.pipeline.stages[i].params  = mStageWidgets[i].stage.params;
    }

    // 同步波段选择 (当前选中文件)
    int row = mFileList->currentRow();
    if (row >= 0 && row < mProject.imageSources.size())
    {
        ImageSource& src = mProject.imageSources[row];

        // 移除旧的波段选择
        src.bandSelections.clear();

        // 多光谱 RGB
        BandSelection msBs;
        msBs.purpose = QStringLiteral("Multispectral");
        bool okR, okG, okB;
        int r = mBandR->text().toInt(&okR);
        int g = mBandG->text().toInt(&okG);
        int b = mBandB->text().toInt(&okB);
        if (okR) msBs.bandNumbers.append(r);
        if (okG) msBs.bandNumbers.append(g);
        if (okB) msBs.bandNumbers.append(b);
        if (!msBs.bandNumbers.isEmpty())
            src.bandSelections.append(msBs);

        // 全色
        BandSelection panBs;
        panBs.purpose = QStringLiteral("Panchromatic");
        bool okPan;
        int pan = mBandPan->text().toInt(&okPan);
        if (okPan)
        {
            panBs.bandNumbers.append(pan);
            src.bandSelections.append(panBs);
        }
    }

    // 同步输出路径
    mProject.output.outputDirectory = mOutputDir->text();
    mProject.output.cleanupIntermediates = mCleanupCheck->isChecked();
    mProject.output.autoConfirm = mAutoConfirmCheck->isChecked();
}
