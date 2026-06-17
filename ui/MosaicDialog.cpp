#include "MosaicDialog.h"

#include <gdal_priv.h>

#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
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
#include <QDialogButtonBox>
#include <cmath>

MosaicDialog::MosaicDialog(QWidget* parent)
    : QDialog(parent)
    {
    setWindowTitle(tr("影像镶嵌与成图 — 参数设置"));
    setMinimumSize(700, 560);
    setupUI();
}

void MosaicDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* tabWidget = new QTabWidget(this);
    tabWidget->addTab(createInputTab(), tr("输入影像"));
    tabWidget->addTab(createColorBalanceTab(), tr("匀色处理"));
    tabWidget->addTab(createSeamlineTab(), tr("拼接线"));
    tabWidget->addTab(createOutputTab(), tr("输出设置"));
    mainLayout->addWidget(tabWidget);

    mStatusLabel = new QLabel(tr("就绪"), this);
    mainLayout->addWidget(mStatusLabel);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &MosaicDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

QWidget* MosaicDialog::createInputTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* listGroup = new QGroupBox(tr("待镶嵌影像列表"), page);
    auto* listLayout = new QVBoxLayout(listGroup);
    mImageList = new QListWidget(listGroup);
    mImageList->setAlternatingRowColors(true);
    mImageList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    listLayout->addWidget(mImageList);

    auto* listBtnLayout = new QHBoxLayout();
    auto* addBtn = new QPushButton(tr("添加影像"), listGroup);
    auto* removeBtn = new QPushButton(tr("移除选中"), listGroup);
    auto* upBtn = new QPushButton(tr("上移"), listGroup);
    auto* downBtn = new QPushButton(tr("下移"), listGroup);
    listBtnLayout->addWidget(addBtn);
    listBtnLayout->addWidget(removeBtn);
    listBtnLayout->addWidget(upBtn);
    listBtnLayout->addWidget(downBtn);
    listBtnLayout->addStretch();
    listLayout->addLayout(listBtnLayout);
    layout->addWidget(listGroup);

    mImageInfoLabel = new QLabel(tr("未加载影像"), page);
    mImageInfoLabel->setWordWrap(true);
    layout->addWidget(mImageInfoLabel);

    connect(addBtn, &QPushButton::clicked, this, &MosaicDialog::onAddImages);
    connect(removeBtn, &QPushButton::clicked, this, &MosaicDialog::onRemoveImages);
    connect(upBtn, &QPushButton::clicked, this, &MosaicDialog::onMoveImageUp);
    connect(downBtn, &QPushButton::clicked, this, &MosaicDialog::onMoveImageDown);

    return page;
}

QWidget* MosaicDialog::createColorBalanceTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* cbGroup = new QGroupBox(tr("匀色方法"), page);
    auto* cbLayout = new QFormLayout(cbGroup);
    mColorBalanceCombo = new QComboBox(cbGroup);
    mColorBalanceCombo->addItems({
        tr("无 (None)"),
        tr("直方图匹配 (Histogram Matching)"),
        tr("Wallis滤波 (Wallis Filter)"),
        tr("查找表 (LUT)")
    });
    cbLayout->addRow(tr("匀色方法:"), mColorBalanceCombo);

    auto* refLayout = new QHBoxLayout();
    mHistRefImagePath = new QLineEdit(cbGroup);
    mHistRefImagePath->setPlaceholderText(tr("选择直方图参考影像..."));
    auto* refBtn = new QPushButton(tr("浏览..."), cbGroup);
    refLayout->addWidget(mHistRefImagePath);
    refLayout->addWidget(refBtn);
    cbLayout->addRow(tr("参考影像:"), refLayout);
    layout->addWidget(cbGroup);

    // Wallis参数组
    mWallisGroup = new QGroupBox(tr("Wallis滤波参数"), page);
    mWallisGroup->setVisible(false);
    auto* wallisLayout = new QFormLayout(mWallisGroup);
    mWallisWindowSpin = new QSpinBox(mWallisGroup);
    mWallisWindowSpin->setRange(15, 511);
    mWallisWindowSpin->setSingleStep(32);
    mWallisWindowSpin->setValue(127);
    wallisLayout->addRow(tr("窗口大小:"), mWallisWindowSpin);

    mWallisContrastSpin = new QDoubleSpinBox(mWallisGroup);
    mWallisContrastSpin->setRange(0.1, 3.0);
    mWallisContrastSpin->setDecimals(1);
    mWallisContrastSpin->setValue(1.0);
    wallisLayout->addRow(tr("对比度系数:"), mWallisContrastSpin);

    mWallisBrightnessSpin = new QDoubleSpinBox(mWallisGroup);
    mWallisBrightnessSpin->setRange(0.1, 2.0);
    mWallisBrightnessSpin->setDecimals(2);
    mWallisBrightnessSpin->setValue(0.5);
    wallisLayout->addRow(tr("亮度系数:"), mWallisBrightnessSpin);
    layout->addWidget(mWallisGroup);

    layout->addStretch();

    connect(mColorBalanceCombo, &QComboBox::currentTextChanged, this, &MosaicDialog::onColorBalanceMethodChanged);
    connect(refBtn, &QPushButton::clicked, this, &MosaicDialog::onSelectHistRefImage);

    return page;
}

QWidget* MosaicDialog::createSeamlineTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* seamGroup = new QGroupBox(tr("拼接线方法"), page);
    auto* seamLayout = new QFormLayout(seamGroup);
    mSeamlineMethodCombo = new QComboBox(seamGroup);
    mSeamlineMethodCombo->addItems({
        tr("无 (直接叠加)"),
        tr("Voronoi 图"),
        tr("最小成本路径 (Min-Cost Path)"),
        tr("手动编辑")
    });
    seamLayout->addRow(tr("拼接线方法:"), mSeamlineMethodCombo);
    layout->addWidget(seamGroup);

    mSeamlineParamsGroup = new QGroupBox(tr("最小成本路径参数"), page);
    mSeamlineParamsGroup->setVisible(false);
    auto* paramLayout = new QFormLayout(mSeamlineParamsGroup);

    mEdgeWeightSpin = new QDoubleSpinBox(mSeamlineParamsGroup);
    mEdgeWeightSpin->setRange(0.0, 10.0);
    mEdgeWeightSpin->setDecimals(1);
    mEdgeWeightSpin->setValue(1.0);
    paramLayout->addRow(tr("边缘代价权重:"), mEdgeWeightSpin);

    mColorWeightSpin = new QDoubleSpinBox(mSeamlineParamsGroup);
    mColorWeightSpin->setRange(0.0, 10.0);
    mColorWeightSpin->setDecimals(1);
    mColorWeightSpin->setValue(1.0);
    paramLayout->addRow(tr("颜色差异权重:"), mColorWeightSpin);

    mTextureWeightSpin = new QDoubleSpinBox(mSeamlineParamsGroup);
    mTextureWeightSpin->setRange(0.0, 10.0);
    mTextureWeightSpin->setDecimals(1);
    mTextureWeightSpin->setValue(0.5);
    paramLayout->addRow(tr("纹理代价权重:"), mTextureWeightSpin);
    layout->addWidget(mSeamlineParamsGroup);

    layout->addStretch();

    connect(mSeamlineMethodCombo, &QComboBox::currentTextChanged, this, [this](const QString& text)
    {
        mSeamlineParamsGroup->setVisible(text.contains(tr("最小成本")));
    });

    return page;
}

QWidget* MosaicDialog::createOutputTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    // 羽化设置
    auto* featherGroup = new QGroupBox(tr("羽化融合"), page);
    auto* featherLayout = new QFormLayout(featherGroup);
    mFeatheringWidthSpin = new QSpinBox(featherGroup);
    mFeatheringWidthSpin->setRange(0, 500);
    mFeatheringWidthSpin->setValue(10);
    mFeatheringWidthSpin->setSuffix(" px");
    featherLayout->addRow(tr("羽化宽度:"), mFeatheringWidthSpin);

    mFeatheringTypeCombo = new QComboBox(featherGroup);
    mFeatheringTypeCombo->addItems({tr("线性 (Linear)"), tr("正弦 (Sine)"), tr("无 (None)")});
    featherLayout->addRow(tr("羽化类型:"), mFeatheringTypeCombo);
    layout->addWidget(featherGroup);

    // 输出范围
    auto* extentGroup = new QGroupBox(tr("输出范围"), page);
    auto* extentLayout = new QFormLayout(extentGroup);
    mUseImageExtentCheck = new QCheckBox(tr("使用影像范围自动计算"), extentGroup);
    mUseImageExtentCheck->setChecked(true);
    extentLayout->addRow(mUseImageExtentCheck);

    mExtentMinXSpin = new QDoubleSpinBox(extentGroup);
    mExtentMinXSpin->setRange(-180.0, 180.0);
    mExtentMinXSpin->setDecimals(6);
    mExtentMinXSpin->setEnabled(false);
    extentLayout->addRow(tr("Min X:"), mExtentMinXSpin);
    mExtentMaxXSpin = new QDoubleSpinBox(extentGroup);
    mExtentMaxXSpin->setRange(-180.0, 180.0);
    mExtentMaxXSpin->setDecimals(6);
    mExtentMaxXSpin->setEnabled(false);
    extentLayout->addRow(tr("Max X:"), mExtentMaxXSpin);
    mExtentMinYSpin = new QDoubleSpinBox(extentGroup);
    mExtentMinYSpin->setRange(-90.0, 90.0);
    mExtentMinYSpin->setDecimals(6);
    mExtentMinYSpin->setEnabled(false);
    extentLayout->addRow(tr("Min Y:"), mExtentMinYSpin);
    mExtentMaxYSpin = new QDoubleSpinBox(extentGroup);
    mExtentMaxYSpin->setRange(-90.0, 90.0);
    mExtentMaxYSpin->setDecimals(6);
    mExtentMaxYSpin->setEnabled(false);
    extentLayout->addRow(tr("Max Y:"), mExtentMaxYSpin);
    layout->addWidget(extentGroup);

    // 输出参数
    auto* outGroup = new QGroupBox(tr("输出参数"), page);
    auto* outLayout = new QFormLayout(outGroup);

    mOutputProjection = new QLineEdit(outGroup);
    mOutputProjection->setPlaceholderText("EPSG:4326");
    outLayout->addRow(tr("输出投影:"), mOutputProjection);

    auto* resLayout = new QHBoxLayout();
    mResXSpin = new QDoubleSpinBox(outGroup);
    mResXSpin->setRange(0.0, 10000.0);
    mResXSpin->setDecimals(6);
    mResXSpin->setValue(0.0);
    mResXSpin->setSpecialValueText(tr("自动"));
    mResXSpin->setToolTip(tr("0 = 自动从源影像检测分辨率"));
    mResYSpin = new QDoubleSpinBox(outGroup);
    mResYSpin->setRange(0.0, 10000.0);
    mResYSpin->setDecimals(6);
    mResYSpin->setValue(0.0);
    mResYSpin->setSpecialValueText(tr("自动"));
    mResYSpin->setToolTip(tr("0 = 自动从源影像检测分辨率"));
    resLayout->addWidget(new QLabel(tr("X:"), outGroup));
    resLayout->addWidget(mResXSpin);
    resLayout->addWidget(new QLabel(tr("Y:"), outGroup));
    resLayout->addWidget(mResYSpin);
    outLayout->addRow(tr("分辨率:"), resLayout);

    mOutputFormatCombo = new QComboBox(outGroup);
    mOutputFormatCombo->addItems({"GeoTIFF", "ENVI", "IMG"});
    outLayout->addRow(tr("输出格式:"), mOutputFormatCombo);

    mBackgroundValueSpin = new QSpinBox(outGroup);
    mBackgroundValueSpin->setRange(0, 255);
    mBackgroundValueSpin->setValue(0);
    outLayout->addRow(tr("背景填充值:"), mBackgroundValueSpin);

    mBlockSizeSpin = new QSpinBox(outGroup);
    mBlockSizeSpin->setRange(64, 4096);
    mBlockSizeSpin->setSingleStep(128);
    mBlockSizeSpin->setValue(512);
    mBlockSizeSpin->setSuffix(" px");
    outLayout->addRow(tr("分块大小:"), mBlockSizeSpin);
    layout->addWidget(outGroup);

    // 输出路径
    auto* pathLayout = new QHBoxLayout();
    mOutputPath = new QLineEdit(outGroup);
    mOutputPath->setPlaceholderText(tr("镶嵌结果输出路径..."));
    auto* pathBtn = new QPushButton(tr("浏览..."), outGroup);
    pathLayout->addWidget(mOutputPath);
    pathLayout->addWidget(pathBtn);
    outLayout->addRow(tr("输出路径:"), pathLayout);

    layout->addStretch();

    connect(mUseImageExtentCheck, &QCheckBox::toggled, this, [this](bool checked)
    {
        mExtentMinXSpin->setEnabled(!checked);
        mExtentMaxXSpin->setEnabled(!checked);
        mExtentMinYSpin->setEnabled(!checked);
        mExtentMaxYSpin->setEnabled(!checked);
    });
    connect(pathBtn, &QPushButton::clicked, this, &MosaicDialog::onSelectOutputPath);

    return page;
}

void MosaicDialog::onAddImages()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("选择待镶嵌遥感影像"),
        QString(),
        tr("遥感影像 (*.tif *.tiff *.img *.dat);;所有文件 (*.*)")
    );
    for (const auto& f : files)
    {
        mImageList->addItem(f);
    }
    mImageInfoLabel->setText(tr("已加载 %1 景影像").arg(mImageList->count()));
}

void MosaicDialog::onRemoveImages()
{
    qDeleteAll(mImageList->selectedItems());
    mImageInfoLabel->setText(tr("已加载 %1 景影像").arg(mImageList->count()));
}

void MosaicDialog::onMoveImageUp()
{
    int row = mImageList->currentRow();
    if (row > 0)
    {
        QListWidgetItem* item = mImageList->takeItem(row);
        mImageList->insertItem(row - 1, item);
        mImageList->setCurrentRow(row - 1);
    }
}

void MosaicDialog::onMoveImageDown()
{
    int row = mImageList->currentRow();
    if (row >= 0 && row < mImageList->count() - 1)
    {
        QListWidgetItem* item = mImageList->takeItem(row);
        mImageList->insertItem(row + 1, item);
        mImageList->setCurrentRow(row + 1);
    }
}

void MosaicDialog::onSelectHistRefImage()
{
    QString file = QFileDialog::getOpenFileName(this, tr("选择直方图参考影像"),
        QString(), tr("遥感影像 (*.tif *.tiff *.img);;所有文件 (*.*)"));
    if (!file.isEmpty()) mHistRefImagePath->setText(file);
}

void MosaicDialog::onSelectOutputPath()
{
    QString file = QFileDialog::getSaveFileName(this, tr("输出镶嵌影像"),
        QString(), tr("GeoTIFF (*.tif *.tiff);;ENVI (*.img *.dat)"));
    if (!file.isEmpty()) mOutputPath->setText(file);
}

void MosaicDialog::onColorBalanceMethodChanged(const QString& method)
{
    mWallisGroup->setVisible(method.contains("Wallis"));
    mHistRefImagePath->setEnabled(method.contains(tr("直方图")));
}

void MosaicDialog::onAccepted()
{
    // TODO: 参数校验由业务逻辑层负责
    accept();
}

void MosaicDialog::setParams(const MosaicParams& params)
{
    mImageList->clear();
    for (const auto& img : params.inputImages)
    {
        mImageList->addItem(img);
    }
    mImageInfoLabel->setText(tr("已加载 %1 景影像").arg(mImageList->count()));

    if (params.colorBalanceMethod == "HistogramMatching") mColorBalanceCombo->setCurrentIndex(1);
    else if (params.colorBalanceMethod == "WallisFilter") mColorBalanceCombo->setCurrentIndex(2);
    else if (params.colorBalanceMethod == "LUT") mColorBalanceCombo->setCurrentIndex(3);
    else mColorBalanceCombo->setCurrentIndex(0);

    mHistRefImagePath->setText(params.histogramReferenceImage);
    mWallisWindowSpin->setValue(params.wallisWindowSize);
    mWallisContrastSpin->setValue(params.wallisContrast);
    mWallisBrightnessSpin->setValue(params.wallisBrightness);

    if (params.seamlineMethod == "Voronoi") mSeamlineMethodCombo->setCurrentIndex(1);
    else if (params.seamlineMethod == "MinCostPath") mSeamlineMethodCombo->setCurrentIndex(2);
    else if (params.seamlineMethod == "Manual") mSeamlineMethodCombo->setCurrentIndex(3);
    else if (params.seamlineMethod == "None") mSeamlineMethodCombo->setCurrentIndex(0);
    else mSeamlineMethodCombo->setCurrentIndex(1);  // 空字符串等未知值默认 Voronoi

    mEdgeWeightSpin->setValue(params.seamlineEdgeWeight);
    mColorWeightSpin->setValue(params.seamlineColorWeight);
    mTextureWeightSpin->setValue(params.seamlineTextureWeight);

    mFeatheringWidthSpin->setValue(params.featheringWidth);
    if (params.featheringType == "Sine") mFeatheringTypeCombo->setCurrentIndex(1);
    else if (params.featheringType == "None") mFeatheringTypeCombo->setCurrentIndex(2);
    else mFeatheringTypeCombo->setCurrentIndex(0);

    mUseImageExtentCheck->setChecked(params.useImageExtent);
    mExtentMinXSpin->setValue(params.outputExtentMinX);
    mExtentMinYSpin->setValue(params.outputExtentMinY);
    mExtentMaxXSpin->setValue(params.outputExtentMaxX);
    mExtentMaxYSpin->setValue(params.outputExtentMaxY);
    mOutputProjection->setText(params.outputProjection);
    mResXSpin->setValue(params.outputResolutionX);
    mResYSpin->setValue(params.outputResolutionY);

    // 自动检测分辨率: 若用户未设置(值为0), 从首张源影像读取
    if (params.outputResolutionX <= 0.0 && !params.inputImages.isEmpty())
    {
        GDALAllRegister();
        GDALDataset* ds = (GDALDataset*)GDALOpen(
            params.inputImages.first().toUtf8(), GA_ReadOnly);
        if (ds)
        {
            double geo[6];
            ds->GetGeoTransform(geo);
            double autoResX = std::abs(geo[1]);
            double autoResY = std::abs(geo[5]);
            mResXSpin->setValue(autoResX);
            mResYSpin->setValue(autoResY);
            GDALClose(ds);
        }
    }

    int fmtIdx = mOutputFormatCombo->findText(params.outputFormat);
    if (fmtIdx >= 0) mOutputFormatCombo->setCurrentIndex(fmtIdx);

    mBackgroundValueSpin->setValue(params.backgroundValue);
    mBlockSizeSpin->setValue(params.blockSize);
    mOutputPath->setText(params.outputPath);
}

MosaicParams MosaicDialog::params() const
{
    MosaicParams p;

    p.inputImages.clear();
    for (int i = 0; i < mImageList->count(); ++i)
    {
        p.inputImages.append(mImageList->item(i)->text());
    }

    int cbIdx = mColorBalanceCombo->currentIndex();
    p.colorBalanceMethod = (cbIdx == 1) ? "HistogramMatching" : (cbIdx == 2) ? "WallisFilter" : (cbIdx == 3) ? "LUT" : "None";

    p.histogramReferenceImage = mHistRefImagePath->text();
    p.wallisWindowSize = mWallisWindowSpin->value();
    p.wallisContrast = mWallisContrastSpin->value();
    p.wallisBrightness = mWallisBrightnessSpin->value();

    int seamIdx = mSeamlineMethodCombo->currentIndex();
    p.seamlineMethod = (seamIdx == 1) ? "Voronoi" : (seamIdx == 2) ? "MinCostPath" : (seamIdx == 3) ? "Manual" : "None";

    p.seamlineEdgeWeight = mEdgeWeightSpin->value();
    p.seamlineColorWeight = mColorWeightSpin->value();
    p.seamlineTextureWeight = mTextureWeightSpin->value();

    p.featheringWidth = mFeatheringWidthSpin->value();
    int ftIdx = mFeatheringTypeCombo->currentIndex();
    p.featheringType = (ftIdx == 1) ? "Sine" : (ftIdx == 2) ? "None" : "Linear";

    p.useImageExtent = mUseImageExtentCheck->isChecked();
    p.outputExtentMinX = mExtentMinXSpin->value();
    p.outputExtentMinY = mExtentMinYSpin->value();
    p.outputExtentMaxX = mExtentMaxXSpin->value();
    p.outputExtentMaxY = mExtentMaxYSpin->value();
    p.outputProjection = mOutputProjection->text();
    p.outputResolutionX = mResXSpin->value();
    p.outputResolutionY = mResYSpin->value();
    p.outputFormat = mOutputFormatCombo->currentText();
    p.backgroundValue = mBackgroundValueSpin->value();
    p.blockSize = mBlockSizeSpin->value();
    p.outputPath = mOutputPath->text();

    return p;
}
