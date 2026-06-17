#include "RadiometricDialog.h"

#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialogButtonBox>

RadiometricDialog::RadiometricDialog(QWidget* parent)
    : QDialog(parent)
    {
    setWindowTitle(tr("辐射定标与大气校正 — 参数设置"));
    setMinimumSize(680, 520);
    setupUI();
}

void RadiometricDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* tabWidget = new QTabWidget(this);
    tabWidget->addTab(createInputTab(), tr("输入设置"));
    tabWidget->addTab(createCalibrationTab(), tr("标定参数"));
    tabWidget->addTab(createAtmosphericTab(), tr("大气校正"));
    tabWidget->addTab(createOutputTab(), tr("输出设置"));
    mainLayout->addWidget(tabWidget);

    mStatusLabel = new QLabel(tr("就绪"), this);
    mainLayout->addWidget(mStatusLabel);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RadiometricDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

QWidget* RadiometricDialog::createInputTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    // 传感器选择
    auto* sensorGroup = new QGroupBox(tr("传感器选择"), page);
    auto* sensorLayout = new QFormLayout(sensorGroup);
    mSensorCombo = new QComboBox(sensorGroup);
    mSensorCombo->addItems({
        tr("Landsat-8 OLI"),
        tr("Landsat-9 OLI/TIRS"),
        tr("Sentinel-2A MSI"),
        tr("Sentinel-2B MSI"),
        tr("高分一号 WFV"),
        tr("高分二号 PMS"),
        tr("高分六号 WFV")
    });
    sensorLayout->addRow(tr("传感器类型:"), mSensorCombo);
    layout->addWidget(sensorGroup);

    // 输入文件列表
    auto* fileGroup = new QGroupBox(tr("输入影像 (支持批量)"), page);
    auto* fileLayout = new QVBoxLayout(fileGroup);
    mInputFileList = new QListWidget(fileGroup);
    mInputFileList->setAlternatingRowColors(true);
    fileLayout->addWidget(mInputFileList);

    auto* fileBtnLayout = new QHBoxLayout();
    auto* addFileBtn = new QPushButton(tr("添加影像"), fileGroup);
    auto* removeFileBtn = new QPushButton(tr("移除选中"), fileGroup);
    fileBtnLayout->addWidget(addFileBtn);
    fileBtnLayout->addWidget(removeFileBtn);
    fileBtnLayout->addStretch();
    fileLayout->addLayout(fileBtnLayout);
    layout->addWidget(fileGroup);

    // 元数据文件
    auto* metaGroup = new QGroupBox(tr("元数据文件"), page);
    auto* metaLayout = new QHBoxLayout(metaGroup);
    mMetadataPath = new QLineEdit(metaGroup);
    mMetadataPath->setPlaceholderText(tr("MTL.txt / MTD_MSIL1C.xml / 自动检测..."));
    auto* metaBtn = new QPushButton(tr("浏览..."), metaGroup);
    metaLayout->addWidget(mMetadataPath);
    metaLayout->addWidget(metaBtn);
    layout->addWidget(metaGroup);

    // 输出目录
    auto* outGroup = new QGroupBox(tr("输出目录"), page);
    auto* outLayout = new QHBoxLayout(outGroup);
    mOutputDir = new QLineEdit(outGroup);
    mOutputDir->setPlaceholderText(tr("选择输出目录..."));
    auto* outDirBtn = new QPushButton(tr("浏览..."), outGroup);
    outLayout->addWidget(mOutputDir);
    outLayout->addWidget(outDirBtn);
    layout->addWidget(outGroup);

    layout->addStretch();

    connect(addFileBtn, &QPushButton::clicked, this, &RadiometricDialog::onAddInputFile);
    connect(removeFileBtn, &QPushButton::clicked, this, &RadiometricDialog::onRemoveInputFile);
    connect(metaBtn, &QPushButton::clicked, this, &RadiometricDialog::onSelectMetadata);
    connect(outDirBtn, &QPushButton::clicked, this, &RadiometricDialog::onSelectOutputDir);
    connect(mSensorCombo, &QComboBox::currentTextChanged, this, &RadiometricDialog::onSensorChanged);

    return page;
}

QWidget* RadiometricDialog::createCalibrationTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* calGroup = new QGroupBox(tr("辐射定标类型"), page);
    auto* calLayout = new QFormLayout(calGroup);
    mCalibrationTypeCombo = new QComboBox(calGroup);
    mCalibrationTypeCombo->addItems({
        tr("DN → 辐亮度 (Radiance)"),
        tr("DN → 表观反射率 (TOA Reflectance)")
    });
    calLayout->addRow(tr("定标类型:"), mCalibrationTypeCombo);
    layout->addWidget(calGroup);

    auto* gainGroup = new QGroupBox(tr("增益/偏置"), page);
    auto* gainLayout = new QVBoxLayout(gainGroup);
    mAutoGainRadio = new QRadioButton(tr("从元数据自动读取"), gainGroup);
    mManualGainRadio = new QRadioButton(tr("手动输入"), gainGroup);
    mAutoGainRadio->setChecked(true);
    gainLayout->addWidget(mAutoGainRadio);
    gainLayout->addWidget(mManualGainRadio);

    auto* manualLayout = new QFormLayout();
    mManualGainSpin = new QDoubleSpinBox(gainGroup);
    mManualGainSpin->setDecimals(6);
    mManualGainSpin->setRange(0.0, 100.0);
    mManualGainSpin->setValue(1.0);
    mManualGainSpin->setEnabled(false);
    manualLayout->addRow(tr("增益 (Gain):"), mManualGainSpin);

    mManualOffsetSpin = new QDoubleSpinBox(gainGroup);
    mManualOffsetSpin->setDecimals(6);
    mManualOffsetSpin->setRange(-100.0, 100.0);
    mManualOffsetSpin->setValue(0.0);
    mManualOffsetSpin->setEnabled(false);
    manualLayout->addRow(tr("偏置 (Offset):"), mManualOffsetSpin);
    gainLayout->addLayout(manualLayout);
    layout->addWidget(gainGroup);

    auto* envGroup = new QGroupBox(tr("观测几何与日地参数"), page);
    auto* envLayout = new QFormLayout(envGroup);
    mSolarZenithSpin = new QDoubleSpinBox(envGroup);
    mSolarZenithSpin->setRange(0.0, 90.0);
    mSolarZenithSpin->setDecimals(2);
    mSolarZenithSpin->setSuffix(tr("°"));
    envLayout->addRow(tr("太阳天顶角:"), mSolarZenithSpin);

    mSolarAzimuthSpin = new QDoubleSpinBox(envGroup);
    mSolarAzimuthSpin->setRange(0.0, 360.0);
    mSolarAzimuthSpin->setDecimals(2);
    mSolarAzimuthSpin->setSuffix(tr("°"));
    envLayout->addRow(tr("太阳方位角:"), mSolarAzimuthSpin);

    mSensorZenithSpin = new QDoubleSpinBox(envGroup);
    mSensorZenithSpin->setRange(0.0, 90.0);
    mSensorZenithSpin->setDecimals(2);
    mSensorZenithSpin->setSuffix(tr("°"));
    envLayout->addRow(tr("观测天顶角:"), mSensorZenithSpin);

    mSensorAzimuthSpin = new QDoubleSpinBox(envGroup);
    mSensorAzimuthSpin->setRange(0.0, 360.0);
    mSensorAzimuthSpin->setDecimals(2);
    mSensorAzimuthSpin->setSuffix(tr("°"));
    envLayout->addRow(tr("观测方位角:"), mSensorAzimuthSpin);

    mEarthSunDistSpin = new QDoubleSpinBox(envGroup);
    mEarthSunDistSpin->setRange(0.9, 1.1);
    mEarthSunDistSpin->setDecimals(4);
    mEarthSunDistSpin->setValue(1.0);
    mEarthSunDistSpin->setSingleStep(0.001);
    envLayout->addRow(tr("日地距离 (AU):"), mEarthSunDistSpin);
    layout->addWidget(envGroup);

    auto* dtypeGroup = new QGroupBox(tr("输出数据类型"), page);
    auto* dtypeLayout = new QFormLayout(dtypeGroup);
    mOutputDataTypeCombo = new QComboBox(dtypeGroup);
    mOutputDataTypeCombo->addItems({"Float32", "UInt16", "Int16"});
    dtypeLayout->addRow(tr("数据类型:"), mOutputDataTypeCombo);
    layout->addWidget(dtypeGroup);

    auto* metaGroup = new QGroupBox(tr("产品元数据 (只读)"), page);
    auto* metaLayout = new QFormLayout(metaGroup);
    mAcqTimeLabel = new QLabel(QStringLiteral("--"), metaGroup);
    mAcqTimeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    metaLayout->addRow(tr("采集时间:"), mAcqTimeLabel);
    mQuantValueLabel = new QLabel(QStringLiteral("--"), metaGroup);
    mQuantValueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    metaLayout->addRow(tr("量化值:"), mQuantValueLabel);
    mAotRefLabel = new QLabel(QStringLiteral("--"), metaGroup);
    mAotRefLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    metaLayout->addRow(tr("参考 AOT (550nm):"), mAotRefLabel);
    mWvRefLabel = new QLabel(QStringLiteral("--"), metaGroup);
    mWvRefLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    metaLayout->addRow(tr("参考水汽 (g/cm²):"), mWvRefLabel);
    layout->addWidget(metaGroup);

    layout->addStretch();

    connect(mManualGainRadio, &QRadioButton::toggled, mManualGainSpin, &QDoubleSpinBox::setEnabled);
    connect(mManualGainRadio, &QRadioButton::toggled, mManualOffsetSpin, &QDoubleSpinBox::setEnabled);

    return page;
}

QWidget* RadiometricDialog::createAtmosphericTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* modelGroup = new QGroupBox(tr("大气校正模型"), page);
    auto* modelLayout = new QFormLayout(modelGroup);
    mAtmModelCombo = new QComboBox(modelGroup);
    mAtmModelCombo->addItems({"6S", "Py6S", "Sen2Cor", tr("无 (跳过大气校正)")});
    modelLayout->addRow(tr("校正模型:"), mAtmModelCombo);
    layout->addWidget(modelGroup);

    // 6S/Py6S 公共参数
    mSixsGroup = new QGroupBox(tr("6S / Py6S 模型参数"), page);
    auto* sixsLayout = new QFormLayout(mSixsGroup);

    mAerosolModelCombo = new QComboBox(mSixsGroup);
    mAerosolModelCombo->addItems({
        tr("大陆型 (Continental)"),
        tr("海洋型 (Maritime)"),
        tr("城市型 (Urban)"),
        tr("沙漠型 (Desert)")
    });
    sixsLayout->addRow(tr("气溶胶模型:"), mAerosolModelCombo);

    mAtmosphericModelCombo = new QComboBox(mSixsGroup);
    mAtmosphericModelCombo->addItems({
        tr("热带 (Tropical)"),
        tr("中纬度夏季 (MidLat Summer)"),
        tr("中纬度冬季 (MidLat Winter)"),
        tr("亚北极夏季 (SubArctic Summer)"),
        tr("亚北极冬季 (SubArctic Winter)")
    });
    sixsLayout->addRow(tr("大气模式:"), mAtmosphericModelCombo);

    mAot550Spin = new QDoubleSpinBox(mSixsGroup);
    mAot550Spin->setRange(0.0, 5.0);
    mAot550Spin->setDecimals(3);
    mAot550Spin->setValue(0.2);
    mAot550Spin->setSingleStep(0.05);
    sixsLayout->addRow(tr("AOT @550nm:"), mAot550Spin);

    mWaterVaporSpin = new QDoubleSpinBox(mSixsGroup);
    mWaterVaporSpin->setRange(0.0, 10.0);
    mWaterVaporSpin->setDecimals(2);
    mWaterVaporSpin->setValue(2.0);
    mWaterVaporSpin->setSuffix(" g/cm²");
    sixsLayout->addRow(tr("水汽含量:"), mWaterVaporSpin);

    mOzoneSpin = new QDoubleSpinBox(mSixsGroup);
    mOzoneSpin->setRange(0.0, 1.0);
    mOzoneSpin->setDecimals(3);
    mOzoneSpin->setValue(0.3);
    mOzoneSpin->setSuffix(" cm-atm");
    sixsLayout->addRow(tr("臭氧含量:"), mOzoneSpin);

    mTargetElevSpin = new QDoubleSpinBox(mSixsGroup);
    mTargetElevSpin->setRange(-1.0, 9.0);
    mTargetElevSpin->setDecimals(2);
    mTargetElevSpin->setSuffix(" km");
    sixsLayout->addRow(tr("目标高程:"), mTargetElevSpin);

    mSensorAltSpin = new QDoubleSpinBox(mSixsGroup);
    mSensorAltSpin->setRange(0.0, 1000.0);
    mSensorAltSpin->setDecimals(0);
    mSensorAltSpin->setValue(800.0);
    mSensorAltSpin->setSuffix(" km");
    sixsLayout->addRow(tr("传感器高度:"), mSensorAltSpin);
    layout->addWidget(mSixsGroup);

    // Sen2Cor 特定参数
    mSen2corGroup = new QGroupBox(tr("Sen2Cor 参数"), page);
    mSen2corGroup->setVisible(false);
    auto* s2cLayout = new QFormLayout(mSen2corGroup);
    mSen2corResSpin = new QSpinBox(mSen2corGroup);
    mSen2corResSpin->setRange(10, 60);
    mSen2corResSpin->setSingleStep(10);
    mSen2corResSpin->setValue(20);
    s2cLayout->addRow(tr("处理分辨率 (m):"), mSen2corResSpin);
    layout->addWidget(mSen2corGroup);

    layout->addStretch();

    connect(mAtmModelCombo, &QComboBox::currentTextChanged, this, &RadiometricDialog::onAtmModelChanged);

    return page;
}

QWidget* RadiometricDialog::createOutputTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* fmtGroup = new QGroupBox(tr("输出设置"), page);
    auto* fmtLayout = new QFormLayout(fmtGroup);

    mOutputFormatCombo = new QComboBox(fmtGroup);
    mOutputFormatCombo->addItems({"ENVI", "GeoTIFF"});
    fmtLayout->addRow(tr("输出格式:"), mOutputFormatCombo);

    mScaleFactorSpin = new QDoubleSpinBox(fmtGroup);
    mScaleFactorSpin->setRange(0.0001, 100000.0);
    mScaleFactorSpin->setDecimals(4);
    mScaleFactorSpin->setValue(1.0);
    fmtLayout->addRow(tr("缩放因子:"), mScaleFactorSpin);

    mNamingPattern = new QLineEdit(fmtGroup);
    mNamingPattern->setPlaceholderText(tr("{SENSOR}_{DATE}_{LEVEL}_result.dat"));
    mNamingPattern->setText(tr("{SENSOR}_{DATE}_{LEVEL}_result"));
    fmtLayout->addRow(tr("命名模板:"), mNamingPattern);
    layout->addWidget(fmtGroup);

    auto* batchGroup = new QGroupBox(tr("批量处理"), page);
    auto* batchLayout = new QFormLayout(batchGroup);
    mBatchModeCheck = new QCheckBox(tr("启用批处理模式"), batchGroup);
    batchLayout->addRow(mBatchModeCheck);
    layout->addWidget(batchGroup);

    layout->addStretch();
    return page;
}

void RadiometricDialog::onAddInputFile()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("选择遥感影像文件"),
        QString(),
        tr("所有支持格式 (*.zip *.SAFE *_MTL.txt *.tif *.tiff *.img *.dat *.hdr);;"
           "Sentinel-2 (*.zip *.SAFE);;"
           "Landsat (*_MTL.txt);;"
           "通用栅格 (*.tif *.tiff *.img);;所有文件 (*.*)")
    );
    for (const auto& f : files)
    {
        mInputFileList->addItem(f);
    }
}

void RadiometricDialog::onRemoveInputFile()
{
    qDeleteAll(mInputFileList->selectedItems());
}

void RadiometricDialog::onSelectOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择输出目录"));
    if (!dir.isEmpty())
    {
        mOutputDir->setText(dir);
    }
}

void RadiometricDialog::onSelectMetadata()
{
    QString file = QFileDialog::getOpenFileName(
        this, tr("选择元数据文件"),
        QString(),
        tr("元数据文件 (*.txt *.xml *.ANG *.MTL);;所有文件 (*.*)")
    );
    if (!file.isEmpty())
    {
        mMetadataPath->setText(file);
    }
}

void RadiometricDialog::onSensorChanged(const QString& sensor)
{
    Q_UNUSED(sensor);
    // TODO: 根据传感器类型自动调整默认参数
}

void RadiometricDialog::onAtmModelChanged(const QString& model)
{
    bool isSixs = model.contains("6S") || model.contains("Py6S");
    bool isSen2cor = model.contains("Sen2Cor");
    mSixsGroup->setVisible(isSixs);
    mSen2corGroup->setVisible(isSen2cor);
}

void RadiometricDialog::onAccepted()
{
    // TODO: 参数校验由业务逻辑层负责
    accept();
}

void RadiometricDialog::setParams(const RadiometricCorrectionParams& params)
{
    int sensorIdx = mSensorCombo->findText(params.sensorType, Qt::MatchContains);
    if (sensorIdx >= 0) mSensorCombo->setCurrentIndex(sensorIdx);

    mInputFileList->clear();
    for (const auto& f : params.inputFiles)
    {
        mInputFileList->addItem(f);
    }

    mMetadataPath->setText(params.metadataFile);
    mOutputDir->setText(params.outputDirectory);

    if (params.calibrationType == "DN2Radiance") mCalibrationTypeCombo->setCurrentIndex(0);
    else mCalibrationTypeCombo->setCurrentIndex(1);

    mAutoGainRadio->setChecked(params.autoGainOffset);
    mManualGainRadio->setChecked(!params.autoGainOffset);
    mManualGainSpin->setValue(params.manualGain);
    mManualOffsetSpin->setValue(params.manualOffset);
    mSolarZenithSpin->setValue(params.solarZenithAngle);
    mSolarAzimuthSpin->setValue(params.solarAzimuthAngle);
    mEarthSunDistSpin->setValue(params.earthSunDistance);
    mSensorZenithSpin->setValue(params.sensorZenithAngle);
    mSensorAzimuthSpin->setValue(params.sensorAzimuthAngle);

    int dtypeIdx = mOutputDataTypeCombo->findText(params.outputDataType);
    if (dtypeIdx >= 0) mOutputDataTypeCombo->setCurrentIndex(dtypeIdx);

    // Match "None" / "无" / model name to the combo items
    int atmIdx = -1;
    if (params.atmModel == "None")
        atmIdx = mAtmModelCombo->findText(QStringLiteral("无"), Qt::MatchContains);
    else
        atmIdx = mAtmModelCombo->findText(params.atmModel, Qt::MatchContains);
    if (atmIdx >= 0) mAtmModelCombo->setCurrentIndex(atmIdx);

    int aeroIdx = mAerosolModelCombo->findText(params.aerosolModel, Qt::MatchContains);
    if (aeroIdx >= 0) mAerosolModelCombo->setCurrentIndex(aeroIdx);

    int atmodIdx = mAtmosphericModelCombo->findText(params.atmosphericModel, Qt::MatchContains);
    if (atmodIdx >= 0) mAtmosphericModelCombo->setCurrentIndex(atmodIdx);

    mAot550Spin->setValue(params.aot550);
    mWaterVaporSpin->setValue(params.waterVapor);
    mOzoneSpin->setValue(params.ozone);
    mTargetElevSpin->setValue(params.targetElevation);
    mSensorAltSpin->setValue(params.sensorAltitude);
    mSen2corResSpin->setValue(params.sen2corResolution);

    int fmtIdx = mOutputFormatCombo->findText(params.outputFormat);
    if (fmtIdx >= 0) mOutputFormatCombo->setCurrentIndex(fmtIdx);
    mScaleFactorSpin->setValue(params.scaleFactor);
    mNamingPattern->setText(params.namingPattern);
    mBatchModeCheck->setChecked(params.batchMode);
}

void RadiometricDialog::setSensorInfo(const SensorInfo& info)
{
    mDisplaySensorInfo = info;

    if (info.acquisitionTime.isValid())
        mAcqTimeLabel->setText(info.acquisitionTime.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
    else
        mAcqTimeLabel->setText(QStringLiteral("--"));

    mQuantValueLabel->setText(QString::number(info.quantificationValue, 'f', 0));

    if (info.meanAOT > 0.0)
    {
        mAotRefLabel->setText(QString::number(info.meanAOT, 'f', 4));
        // 如果用户未手动修改 AOT（仍为默认 0.2），自动填入元数据中的参考值
        if (qAbs(mAot550Spin->value() - 0.2) < 1e-6)
            mAot550Spin->setValue(info.meanAOT);
    }
    else
    {
        mAotRefLabel->setText(QStringLiteral("--"));
    }

    if (info.meanWV > 0.0)
    {
        mWvRefLabel->setText(QString::number(info.meanWV, 'f', 3));
        // 如果用户未手动修改水汽（仍为默认 2.0），自动填入元数据中的参考值
        if (qAbs(mWaterVaporSpin->value() - 2.0) < 1e-6)
            mWaterVaporSpin->setValue(info.meanWV);
    }
    else
    {
        mWvRefLabel->setText(QStringLiteral("--"));
    }
}

RadiometricCorrectionParams RadiometricDialog::params() const
{
    RadiometricCorrectionParams p;
    p.sensorType = mSensorCombo->currentText();

    p.inputFiles.clear();
    for (int i = 0; i < mInputFileList->count(); ++i)
    {
        p.inputFiles.append(mInputFileList->item(i)->text());
    }

    p.metadataFile = mMetadataPath->text();
    p.outputDirectory = mOutputDir->text();
    p.calibrationType = (mCalibrationTypeCombo->currentIndex() == 0) ? "DN2Radiance" : "DN2Reflectance";
    p.autoGainOffset = mAutoGainRadio->isChecked();
    p.manualGain = mManualGainSpin->value();
    p.manualOffset = mManualOffsetSpin->value();
    p.solarZenithAngle = mSolarZenithSpin->value();
    p.solarAzimuthAngle = mSolarAzimuthSpin->value();
    p.earthSunDistance = mEarthSunDistSpin->value();
    p.sensorZenithAngle = mSensorZenithSpin->value();
    p.sensorAzimuthAngle = mSensorAzimuthSpin->value();
    p.outputDataType = mOutputDataTypeCombo->currentText();

    QString atmText = mAtmModelCombo->currentText();
    if (atmText.contains("6S") && !atmText.contains("Py")) p.atmModel = "6S";
    else if (atmText.contains("Py6S")) p.atmModel = "Py6S";
    else if (atmText.contains("Sen2Cor")) p.atmModel = "Sen2Cor";
    else p.atmModel = "None";

    p.aerosolModel = mAerosolModelCombo->currentText();
    p.atmosphericModel = mAtmosphericModelCombo->currentText();
    p.aot550 = mAot550Spin->value();
    p.waterVapor = mWaterVaporSpin->value();
    p.ozone = mOzoneSpin->value();
    p.targetElevation = mTargetElevSpin->value();
    p.sensorAltitude = mSensorAltSpin->value();
    p.sen2corResolution = mSen2corResSpin->value();

    p.outputFormat = mOutputFormatCombo->currentText();
    p.scaleFactor = mScaleFactorSpin->value();
    p.namingPattern = mNamingPattern->text();
    p.batchMode = mBatchModeCheck->isChecked();

    return p;
}
