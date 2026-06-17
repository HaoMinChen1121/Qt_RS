#include "FusionDialog.h"

#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QStackedWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QDialogButtonBox>

FusionDialog::FusionDialog(QWidget* parent)
    : QDialog(parent)
    {
    setWindowTitle(tr("图像融合 — 参数设置"));
    setMinimumSize(640, 520);
    setupUI();
}

void FusionDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* tabWidget = new QTabWidget(this);
    tabWidget->addTab(createInputTab(), tr("输入数据"));
    tabWidget->addTab(createAlgorithmTab(), tr("算法参数"));
    tabWidget->addTab(createQualityTab(), tr("质量评价"));
    mainLayout->addWidget(tabWidget);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &FusionDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

QWidget* FusionDialog::createInputTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* panGroup = new QGroupBox(tr("全色影像 (Panchromatic)"), page);
    auto* panLayout = new QHBoxLayout(panGroup);
    mPanImagePath = new QLineEdit(panGroup);
    mPanImagePath->setPlaceholderText(tr("选择高分辨率全色波段影像..."));
    auto* panBtn = new QPushButton(tr("浏览..."), panGroup);
    panLayout->addWidget(mPanImagePath);
    panLayout->addWidget(panBtn);
    layout->addWidget(panGroup);

    auto* msGroup = new QGroupBox(tr("多光谱影像 (Multispectral)"), page);
    auto* msLayout = new QHBoxLayout(msGroup);
    mMsImagePath = new QLineEdit(msGroup);
    mMsImagePath->setPlaceholderText(tr("选择低分辨率多光谱影像..."));
    auto* msBtn = new QPushButton(tr("浏览..."), msGroup);
    msLayout->addWidget(mMsImagePath);
    msLayout->addWidget(msBtn);
    layout->addWidget(msGroup);

    auto* outGroup = new QGroupBox(tr("输出路径"), page);
    auto* outLayout = new QHBoxLayout(outGroup);
    mOutputPath = new QLineEdit(outGroup);
    mOutputPath->setPlaceholderText(tr("融合结果输出路径..."));
    auto* outBtn = new QPushButton(tr("浏览..."), outGroup);
    outLayout->addWidget(mOutputPath);
    outLayout->addWidget(outBtn);
    layout->addWidget(outGroup);

    layout->addStretch();

    connect(panBtn, &QPushButton::clicked, this, &FusionDialog::onSelectPanImage);
    connect(msBtn, &QPushButton::clicked, this, &FusionDialog::onSelectMsImage);
    connect(outBtn, &QPushButton::clicked, this, &FusionDialog::onSelectOutputPath);

    return page;
}

QWidget* FusionDialog::createAlgorithmTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* algoGroup = new QGroupBox(tr("融合算法"), page);
    auto* algoLayout = new QFormLayout(algoGroup);
    mAlgorithmCombo = new QComboBox(algoGroup);
    mAlgorithmCombo->addItems({"IHS", "Brovey", "Gram-Schmidt", "PCA", "HPF", "Wavelet"});
    algoLayout->addRow(tr("算法选择:"), mAlgorithmCombo);
    layout->addWidget(algoGroup);

    // 参数堆栈 - 根据算法切换显示
    mAlgorithmParamsStack = new QStackedWidget(page);

    // IHS 参数页
    auto* ihsPage = new QWidget(mAlgorithmParamsStack);
    auto* ihsLayout = new QFormLayout(ihsPage);
    mIhsColorModelCombo = new QComboBox(ihsPage);
    mIhsColorModelCombo->addItems({"HSI", "HSV", "RGB"});
    ihsLayout->addRow(tr("色彩模型:"), mIhsColorModelCombo);
    mIhsStretchCombo = new QComboBox(ihsPage);
    mIhsStretchCombo->addItems({tr("线性拉伸"), tr("百分比截断"), tr("标准差拉伸")});
    ihsLayout->addRow(tr("拉伸方式:"), mIhsStretchCombo);
    mAlgorithmParamsStack->addWidget(ihsPage);

    // Brovey 参数页
    auto* broveyPage = new QWidget(mAlgorithmParamsStack);
    auto* broveyLayout = new QFormLayout(broveyPage);
    mBroveyWeightsEdit = new QLineEdit(broveyPage);
    mBroveyWeightsEdit->setPlaceholderText(tr("例如: 0.3, 0.3, 0.4"));
    mBroveyWeightsEdit->setText("0.333, 0.333, 0.334");
    broveyLayout->addRow(tr("波段权重:"), mBroveyWeightsEdit);
    mAlgorithmParamsStack->addWidget(broveyPage);

    // Gram-Schmidt 参数页
    auto* gsPage = new QWidget(mAlgorithmParamsStack);
    auto* gsLayout = new QFormLayout(gsPage);
    mGsSimMethodCombo = new QComboBox(gsPage);
    mGsSimMethodCombo->addItems({tr("均值模拟 (Average)"), tr("光谱响应模拟 (Spectral Response)")});
    gsLayout->addRow(tr("模拟全色方法:"), mGsSimMethodCombo);
    mGsSensorType = new QLineEdit(gsPage);
    mGsSensorType->setPlaceholderText(tr("例如: IKONOS, QuickBird, GF-2..."));
    gsLayout->addRow(tr("传感器类型:"), mGsSensorType);
    mAlgorithmParamsStack->addWidget(gsPage);

    // PCA 参数页
    auto* pcaPage = new QWidget(mAlgorithmParamsStack);
    auto* pcaLayout = new QFormLayout(pcaPage);
    mPcaComponentSpin = new QSpinBox(pcaPage);
    mPcaComponentSpin->setRange(1, 10);
    mPcaComponentSpin->setValue(1);
    pcaLayout->addRow(tr("保留主成分数:"), mPcaComponentSpin);
    mAlgorithmParamsStack->addWidget(pcaPage);

    // HPF 参数页
    auto* hpfPage = new QWidget(mAlgorithmParamsStack);
    auto* hpfLayout = new QFormLayout(hpfPage);
    mHpfKernelSpin = new QSpinBox(hpfPage);
    mHpfKernelSpin->setRange(3, 31);
    mHpfKernelSpin->setSingleStep(2);
    mHpfKernelSpin->setValue(5);
    hpfLayout->addRow(tr("滤波核大小:"), mHpfKernelSpin);
    mHpfWeightSpin = new QDoubleSpinBox(hpfPage);
    mHpfWeightSpin->setRange(0.0, 2.0);
    mHpfWeightSpin->setDecimals(2);
    mHpfWeightSpin->setValue(0.5);
    mHpfWeightSpin->setSingleStep(0.1);
    hpfLayout->addRow(tr("高通权重:"), mHpfWeightSpin);
    mAlgorithmParamsStack->addWidget(hpfPage);

    // Wavelet 参数页
    auto* waveletPage = new QWidget(mAlgorithmParamsStack);
    auto* waveletLayout = new QFormLayout(waveletPage);
    mWaveletLevelSpin = new QSpinBox(waveletPage);
    mWaveletLevelSpin->setRange(1, 10);
    mWaveletLevelSpin->setValue(3);
    waveletLayout->addRow(tr("分解级数:"), mWaveletLevelSpin);
    mWaveletTypeCombo = new QComboBox(waveletPage);
    mWaveletTypeCombo->addItems({"Daubechies4", "Haar", "Symlet8", "Coiflet5"});
    waveletLayout->addRow(tr("小波类型:"), mWaveletTypeCombo);
    mAlgorithmParamsStack->addWidget(waveletPage);

    layout->addWidget(mAlgorithmParamsStack);
    layout->addStretch();

    connect(mAlgorithmCombo, &QComboBox::currentTextChanged, this, &FusionDialog::onAlgorithmChanged);

    return page;
}

QWidget* FusionDialog::createQualityTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* metricsGroup = new QGroupBox(tr("评价指标"), page);
    auto* metricsLayout = new QVBoxLayout(metricsGroup);

    mCheckCorrCoeff = new QCheckBox(tr("相关系数 (Correlation Coefficient)"), metricsGroup);
    mCheckCorrCoeff->setChecked(true);
    mCheckAvgGradient = new QCheckBox(tr("平均梯度 (Average Gradient)"), metricsGroup);
    mCheckAvgGradient->setChecked(true);
    mCheckRMSE = new QCheckBox(tr("均方根误差 (RMSE)"), metricsGroup);
    mCheckERGAS = new QCheckBox(tr("ERGAS 综合指标"), metricsGroup);
    mCheckSAM = new QCheckBox(tr("光谱角映射 (SAM)"), metricsGroup);
    mCheckSSIM = new QCheckBox(tr("结构相似性 (SSIM)"), metricsGroup);
    mCheckUIQI = new QCheckBox(tr("通用图像质量指标 (UIQI)"), metricsGroup);

    metricsLayout->addWidget(mCheckCorrCoeff);
    metricsLayout->addWidget(mCheckAvgGradient);
    metricsLayout->addWidget(mCheckRMSE);
    metricsLayout->addWidget(mCheckERGAS);
    metricsLayout->addWidget(mCheckSAM);
    metricsLayout->addWidget(mCheckSSIM);
    metricsLayout->addWidget(mCheckUIQI);
    layout->addWidget(metricsGroup);

    // 结果表格
    mQualityTable = new QTableWidget(0, 3, page);
    mQualityTable->setHorizontalHeaderLabels({tr("指标"), tr("融合前"), tr("融合后")});
    mQualityTable->horizontalHeader()->setStretchLastSection(true);
    mQualityTable->setAlternatingRowColors(true);
    layout->addWidget(mQualityTable);

    mQualityStatus = new QLabel(tr("选择评价指标后，执行融合将自动计算并显示结果"), page);
    layout->addWidget(mQualityStatus);

    return page;
}

void FusionDialog::onSelectPanImage()
{
    QString file = QFileDialog::getOpenFileName(this, tr("选择全色影像"),
        QString(), tr("遥感影像 (*.tif *.tiff *.img);;所有文件 (*.*)"));
    if (!file.isEmpty()) mPanImagePath->setText(file);
}

void FusionDialog::onSelectMsImage()
{
    QString file = QFileDialog::getOpenFileName(this, tr("选择多光谱影像"),
        QString(), tr("遥感影像 (*.tif *.tiff *.img);;所有文件 (*.*)"));
    if (!file.isEmpty()) mMsImagePath->setText(file);
}

void FusionDialog::onSelectOutputPath()
{
    QString file = QFileDialog::getSaveFileName(this, tr("输出融合影像"),
        QString(), tr("GeoTIFF (*.tif *.tiff);;ENVI (*.img *.dat)"));
    if (!file.isEmpty()) mOutputPath->setText(file);
}

void FusionDialog::onAlgorithmChanged(const QString& algo)
{
    if (algo == "IHS") mAlgorithmParamsStack->setCurrentIndex(0);
    else if (algo == "Brovey") mAlgorithmParamsStack->setCurrentIndex(1);
    else if (algo == "Gram-Schmidt") mAlgorithmParamsStack->setCurrentIndex(2);
    else if (algo == "PCA") mAlgorithmParamsStack->setCurrentIndex(3);
    else if (algo == "HPF") mAlgorithmParamsStack->setCurrentIndex(4);
    else if (algo == "Wavelet") mAlgorithmParamsStack->setCurrentIndex(5);
}

void FusionDialog::onAccepted()
{
    // TODO: 参数校验由业务逻辑层负责
    accept();
}

void FusionDialog::setParams(const ImageFusionParams& params)
{
    mPanImagePath->setText(params.panchromaticImage);
    mMsImagePath->setText(params.multispectralImage);
    mOutputPath->setText(params.outputPath);

    if (params.algorithm == "Brovey") mAlgorithmCombo->setCurrentIndex(1);
    else if (params.algorithm == "GramSchmidt") mAlgorithmCombo->setCurrentIndex(2);
    else if (params.algorithm == "PCA") mAlgorithmCombo->setCurrentIndex(3);
    else if (params.algorithm == "HPF") mAlgorithmCombo->setCurrentIndex(4);
    else if (params.algorithm == "Wavelet") mAlgorithmCombo->setCurrentIndex(5);
    else mAlgorithmCombo->setCurrentIndex(0);

    mIhsColorModelCombo->setCurrentText(params.ihsColorModel);
    mIhsStretchCombo->setCurrentText(params.ihsStretchType);

    QStringList weightStrs;
    for (double w : params.broveyBandWeights) weightStrs << QString::number(w, 'f', 3);
    mBroveyWeightsEdit->setText(weightStrs.join(", "));

    mGsSimMethodCombo->setCurrentIndex(params.gsSimulationMethod == "SpectralResponse" ? 1 : 0);
    mGsSensorType->setText(params.gsSensorType);
    mPcaComponentSpin->setValue(params.pcaComponentCount);
    mHpfKernelSpin->setValue(params.hpfKernelSize);
    mHpfWeightSpin->setValue(params.hpfWeight);
    mWaveletLevelSpin->setValue(params.waveletDecompositionLevel);
    mWaveletTypeCombo->setCurrentText(params.waveletType);

    mCheckCorrCoeff->setChecked(params.computeCorrelationCoefficient);
    mCheckAvgGradient->setChecked(params.computeAverageGradient);
    mCheckRMSE->setChecked(params.computeRMSE);
    mCheckERGAS->setChecked(params.computeERGAS);
    mCheckSAM->setChecked(params.computeSAM);
    mCheckSSIM->setChecked(params.computeSSIM);
    mCheckUIQI->setChecked(params.computeUIQI);
}

ImageFusionParams FusionDialog::params() const
{
    ImageFusionParams p;
    p.panchromaticImage = mPanImagePath->text();
    p.multispectralImage = mMsImagePath->text();
    p.outputPath = mOutputPath->text();

    QString algo = mAlgorithmCombo->currentText();
    if (algo == "IHS") p.algorithm = "IHS";
    else if (algo == "Brovey") p.algorithm = "Brovey";
    else if (algo == "Gram-Schmidt") p.algorithm = "GramSchmidt";
    else if (algo == "PCA") p.algorithm = "PCA";
    else if (algo == "HPF") p.algorithm = "HPF";
    else if (algo == "Wavelet") p.algorithm = "Wavelet";

    p.ihsColorModel = mIhsColorModelCombo->currentText();
    p.ihsStretchType = mIhsStretchCombo->currentText();

    QStringList weightStrs = mBroveyWeightsEdit->text().split(",", Qt::SkipEmptyParts);
    p.broveyBandWeights.clear();
    for (const auto& s : weightStrs) p.broveyBandWeights.append(s.trimmed().toDouble());

    p.gsSimulationMethod = (mGsSimMethodCombo->currentIndex() == 1) ? "SpectralResponse" : "Average";
    p.gsSensorType = mGsSensorType->text();
    p.pcaComponentCount = mPcaComponentSpin->value();
    p.hpfKernelSize = mHpfKernelSpin->value();
    p.hpfWeight = mHpfWeightSpin->value();
    p.waveletDecompositionLevel = mWaveletLevelSpin->value();
    p.waveletType = mWaveletTypeCombo->currentText();

    p.computeCorrelationCoefficient = mCheckCorrCoeff->isChecked();
    p.computeAverageGradient = mCheckAvgGradient->isChecked();
    p.computeRMSE = mCheckRMSE->isChecked();
    p.computeERGAS = mCheckERGAS->isChecked();
    p.computeSAM = mCheckSAM->isChecked();
    p.computeSSIM = mCheckSSIM->isChecked();
    p.computeUIQI = mCheckUIQI->isChecked();

    return p;
}

void FusionDialog::setQualityMetrics(const FusionQualityMetrics& metrics)
{
    mQualityTable->setRowCount(0);
    auto addRow = [this](const QString& name, double before, double after)
    {
        int r = mQualityTable->rowCount();
        mQualityTable->insertRow(r);
        mQualityTable->setItem(r, 0, new QTableWidgetItem(name));
        mQualityTable->setItem(r, 1, new QTableWidgetItem(QString::number(before, 'f', 4)));
        mQualityTable->setItem(r, 2, new QTableWidgetItem(QString::number(after, 'f', 4)));
    };

    if (mCheckCorrCoeff->isChecked()) addRow(tr("相关系数"), 0.0, metrics.correlationCoefficient);
    if (mCheckAvgGradient->isChecked()) addRow(tr("平均梯度"), 0.0, metrics.averageGradient);
    if (mCheckRMSE->isChecked()) addRow("RMSE", 0.0, metrics.rmse);
    if (mCheckERGAS->isChecked()) addRow("ERGAS", 0.0, metrics.ergas);
    if (mCheckSAM->isChecked()) addRow("SAM", 0.0, metrics.sam);
    if (mCheckSSIM->isChecked()) addRow("SSIM", 0.0, metrics.ssim);
    if (mCheckUIQI->isChecked()) addRow("UIQI", 0.0, metrics.uiqi);

    mQualityStatus->setText(tr("质量评价计算完成"));
}

void FusionDialog::showIhsParams(bool visible) { Q_UNUSED(visible); }
void FusionDialog::showBroveyParams(bool visible) { Q_UNUSED(visible); }
void FusionDialog::showGsParams(bool visible) { Q_UNUSED(visible); }
void FusionDialog::showPcaParams(bool visible) { Q_UNUSED(visible); }
void FusionDialog::showHpfParams(bool visible) { Q_UNUSED(visible); }
void FusionDialog::showWaveletParams(bool visible) { Q_UNUSED(visible); }
