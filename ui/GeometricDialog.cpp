#include "GeometricDialog.h"
#include "algorithms/geometric/GeometricCorrector.h"
#include "algorithms/geometric/GcpMatcher.h"
#include "algorithms/geometric/GcpModelSolver.h"
#include "domain/params/GeometricCorrectionParams.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>

#include <gdal.h>
#include <QTableWidget>
#include <QVBoxLayout>

static QPair<double, double> readPixelSize(const QString& path);

GeometricDialog::GeometricDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
}

void GeometricDialog::setInput(const GeometricInput& input)
{
    mInput = input;
    mSrcPath->setText(input.sourceImage);
    mRefPath->setText(input.referenceImage);

    // 自动读取参考影像像元大小作为输出默认值
    QString refImg = input.referenceImage.isEmpty() ? input.sourceImage : input.referenceImage;
    auto ps = readPixelSize(refImg);
    if (ps.first > 0)  mPxSizeX->setValue(ps.first);
    if (ps.second > 0) mPxSizeY->setValue(ps.second);
}

GeometricInput GeometricDialog::inputParams() const
{
    return mInput;
}

// ===================================================================
//  setupUI
// ===================================================================
void GeometricDialog::setupUI()
{
    setWindowTitle(tr("几何精校正"));
    resize(700, 550);

    auto* mainLayout = new QVBoxLayout(this);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(createInputTab(), tr("输入数据"));
    tabs->addTab(createGcpTab(),   tr("控制点列表"));
    tabs->addTab(createModelTab(), tr("校正模型"));
    mainLayout->addWidget(tabs);

    // Bottom buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* btnOk = new QPushButton(tr("确定"), this);
    auto* btnCancel = new QPushButton(tr("取消"), this);
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);

    connect(btnOk, &QPushButton::clicked, this, &GeometricDialog::onAccepted);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

// ===================================================================
//  Tab 1: 输入数据
// ===================================================================
QWidget* GeometricDialog::createInputTab()
{
    auto* w = new QWidget(this);
    auto* lay = new QFormLayout(w);

    // Source image
    auto* srcRow = new QHBoxLayout();
    mSrcPath = new QLineEdit(w);
    mSrcPath->setReadOnly(true);
    auto* btnSrc = new QPushButton(tr("浏览..."), w);
    srcRow->addWidget(mSrcPath);
    srcRow->addWidget(btnSrc);
    lay->addRow(tr("待校正影像:"), srcRow);
    connect(btnSrc, &QPushButton::clicked, this, &GeometricDialog::onSelectSourceImage);

    // Reference image
    auto* refRow = new QHBoxLayout();
    mRefPath = new QLineEdit(w);
    mRefPath->setReadOnly(true);
    auto* btnRef = new QPushButton(tr("浏览..."), w);
    refRow->addWidget(mRefPath);
    refRow->addWidget(btnRef);
    lay->addRow(tr("参考影像:"), refRow);
    connect(btnRef, &QPushButton::clicked, this, &GeometricDialog::onSelectReferenceImage);

    // Reference type
    mRefTypeCombo = new QComboBox(w);
    mRefTypeCombo->addItems({tr("参考影像"), tr("GCP文件"), tr("手动坐标")});
    lay->addRow(tr("参考类型:"), mRefTypeCombo);

    // Output path
    auto* outRow = new QHBoxLayout();
    mOutputPath = new QLineEdit(w);
    auto* btnOut = new QPushButton(tr("浏览..."), w);
    outRow->addWidget(mOutputPath);
    outRow->addWidget(btnOut);
    lay->addRow(tr("输出路径:"), outRow);
    connect(btnOut, &QPushButton::clicked, this, &GeometricDialog::onSelectOutputPath);

    return w;
}

// ===================================================================
//  Tab 2: 控制点列表
// ===================================================================
QWidget* GeometricDialog::createGcpTab()
{
    auto* w = new QWidget(this);
    auto* lay = new QVBoxLayout(w);

    // Toolbar
    auto* tb = new QHBoxLayout();
    auto* lblMethod = new QLabel(tr("匹配算法:"), w);
    auto* cmbMethod = new QComboBox(w);
    cmbMethod->addItems({"SIFT", "SURF", "NCC"});
    cmbMethod->setCurrentText(mInput.matchingAlgorithm);
    QObject::connect(cmbMethod, &QComboBox::currentTextChanged, this,
        [this](const QString& t) { mInput.matchingAlgorithm = t; });
    auto* btnAuto = new QPushButton(tr("自动检测"), w);
    auto* btnAdd  = new QPushButton(tr("添加"), w);
    auto* btnDel  = new QPushButton(tr("删除"), w);
    auto* btnImp  = new QPushButton(tr("导入..."), w);
    auto* btnExp  = new QPushButton(tr("导出..."), w);
    tb->addWidget(lblMethod);
    tb->addWidget(cmbMethod);
    tb->addWidget(btnAuto);
    tb->addWidget(btnAdd);
    tb->addWidget(btnDel);
    tb->addWidget(btnImp);
    tb->addWidget(btnExp);
    tb->addStretch();
    lay->addLayout(tb);

    connect(btnAuto, &QPushButton::clicked, this, &GeometricDialog::onAutoDetect);
    connect(btnAdd,  &QPushButton::clicked, this, &GeometricDialog::onAddGcp);
    connect(btnDel,  &QPushButton::clicked, this, &GeometricDialog::onDeleteGcp);
    connect(btnImp,  &QPushButton::clicked, this, &GeometricDialog::onImportGcp);
    connect(btnExp,  &QPushButton::clicked, this, &GeometricDialog::onExportGcp);

    // Table
    mGcpTable = new QTableWidget(0, 6, w);
    mGcpTable->setHorizontalHeaderLabels(
        {tr("编号"), tr("源 X"), tr("源 Y"), tr("参考 X"), tr("参考 Y"), tr("残差")});
    mGcpTable->horizontalHeader()->setStretchLastSection(true);
    mGcpTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    lay->addWidget(mGcpTable);

    // Status
    auto* statusRow = new QHBoxLayout();
    mLblGcpCount = new QLabel(tr("控制点数量: 0"), w);
    mLblRms = new QLabel(tr("总体 RMSE: N/A"), w);
    statusRow->addWidget(mLblGcpCount);
    statusRow->addWidget(mLblRms);
    statusRow->addStretch();
    lay->addLayout(statusRow);

    return w;
}

// ===================================================================
//  Tab 3: 校正模型
// ===================================================================
QWidget* GeometricDialog::createModelTab()
{
    auto* w = new QWidget(this);
    auto* lay = new QFormLayout(w);

    // Correction model
    mModelCombo = new QComboBox(w);
    mModelCombo->addItems({"Polynomial1", "Polynomial2", "Polynomial3", "TPS"});
    mModelCombo->setCurrentIndex(1); // Polynomial2
    lay->addRow(tr("校正模型:"), mModelCombo);

    // Polynomial order — synced with model selection
    mPolyOrderSpin = new QSpinBox(w);
    mPolyOrderSpin->setRange(1, 5);
    mPolyOrderSpin->setValue(2);
    lay->addRow(tr("多项式阶数:"), mPolyOrderSpin);

    connect(mModelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this](int idx) {
            if (idx == 3) { // TPS
                mPolyOrderSpin->setEnabled(false);
            } else {
                mPolyOrderSpin->setEnabled(true);
                mPolyOrderSpin->setValue(idx + 1); // Polynomial1→1, Polynomial2→2, etc.
            }
        });

    // Resampling
    mResampleCombo = new QComboBox(w);
    mResampleCombo->addItems({tr("最邻近"), tr("双线性"), tr("三次卷积")});
    lay->addRow(tr("重采样方法:"), mResampleCombo);

    // Output projection
    mOutProj = new QLineEdit(w);
    mOutProj->setPlaceholderText(tr("例如: EPSG:32650 或留空使用源投影"));
    lay->addRow(tr("输出投影:"), mOutProj);

    // Pixel size
    auto* pxRow = new QHBoxLayout();
    mPxSizeX = new QDoubleSpinBox(w);
    mPxSizeY = new QDoubleSpinBox(w);
    mPxSizeX->setRange(0.1, 1000.0); mPxSizeX->setDecimals(2);
    mPxSizeY->setRange(0.1, 1000.0); mPxSizeY->setDecimals(2);
    pxRow->addWidget(new QLabel(tr("X:"), w));
    pxRow->addWidget(mPxSizeX);
    pxRow->addWidget(new QLabel(tr("Y:"), w));
    pxRow->addWidget(mPxSizeY);
    lay->addRow(tr("输出像元大小:"), pxRow);

    // Output extent
    auto* extGroup = new QGroupBox(tr("输出范围（留空 = 自动计算）"), w);
    auto* extLay = new QFormLayout(extGroup);
    auto* minRow = new QHBoxLayout();
    mExtMinX = new QDoubleSpinBox(w); mExtMinY = new QDoubleSpinBox(w);
    mExtMinX->setRange(-1e9, 1e9); mExtMinX->setDecimals(2); mExtMinX->setValue(0.0);
    mExtMinY->setRange(-1e9, 1e9); mExtMinY->setDecimals(2); mExtMinY->setValue(0.0);
    minRow->addWidget(new QLabel(tr("MinX:"), w)); minRow->addWidget(mExtMinX);
    minRow->addWidget(new QLabel(tr("MinY:"), w)); minRow->addWidget(mExtMinY);
    extLay->addRow(minRow);

    auto* maxRow = new QHBoxLayout();
    mExtMaxX = new QDoubleSpinBox(w); mExtMaxY = new QDoubleSpinBox(w);
    mExtMaxX->setRange(-1e9, 1e9); mExtMaxX->setDecimals(2); mExtMaxX->setValue(0.0);
    mExtMaxY->setRange(-1e9, 1e9); mExtMaxY->setDecimals(2); mExtMaxY->setValue(0.0);
    maxRow->addWidget(new QLabel(tr("MaxX:"), w)); maxRow->addWidget(mExtMaxX);
    maxRow->addWidget(new QLabel(tr("MaxY:"), w)); maxRow->addWidget(mExtMaxY);
    extLay->addRow(maxRow);

    lay->addRow(extGroup);

    return w;
}

// ===================================================================
//  Slots
// ===================================================================

static QPair<double, double> readPixelSize(const QString& path)
{
    GDALAllRegister();
    GDALDatasetH hDS = GDALOpen(path.toUtf8().constData(), GA_ReadOnly);
    if (!hDS)
        return {0, 0};
    double gt[6];
    QPair<double, double> ps{0, 0};
    if (GDALGetGeoTransform(hDS, gt) == CE_None)
        ps = {std::abs(gt[1]), std::abs(gt[5])};
    GDALClose(hDS);
    return ps;
}

void GeometricDialog::onSelectSourceImage()
{
    QString f = QFileDialog::getOpenFileName(this, tr("选择待校正影像"),
        QString(), tr("遥感影像 (*.tif *.tiff *.img);;所有文件 (*.*)"));
    if (!f.isEmpty()) {
        mSrcPath->setText(f);
        mInput.sourceImage = f;
        // If reference not yet loaded, use source pixel size as initial default
        if (mInput.referenceImage.isEmpty()) {
            auto ps = readPixelSize(f);
            if (ps.first > 0)  mPxSizeX->setValue(ps.first);
            if (ps.second > 0) mPxSizeY->setValue(ps.second);
        }
    }
}

void GeometricDialog::onSelectReferenceImage()
{
    QString f = QFileDialog::getOpenFileName(this, tr("选择参考影像"),
        QString(), tr("遥感影像 (*.tif *.tiff *.img);;所有文件 (*.*)"));
    if (!f.isEmpty()) {
        mRefPath->setText(f);
        mInput.referenceImage = f;
        // Default output pixel size to reference image resolution
        auto ps = readPixelSize(f);
        if (ps.first > 0)  mPxSizeX->setValue(ps.first);
        if (ps.second > 0) mPxSizeY->setValue(ps.second);
    }
}

void GeometricDialog::onSelectOutputPath()
{
    QString f = QFileDialog::getSaveFileName(this, tr("输出路径"),
        QString(), tr("GeoTIFF (*.tif *.tiff)"));
    if (!f.isEmpty())
        mOutputPath->setText(f);
}

void GeometricDialog::onAddGcp()
{
    QMessageBox::information(this, tr("提示"),
        tr("手动添加控制点功能正在开发中。\n请在表格中直接输入源坐标和参考坐标。"));
}

void GeometricDialog::onDeleteGcp()
{
    int row = mGcpTable->currentRow();
    if (row >= 0)
        mGcpTable->removeRow(row);
    mLblGcpCount->setText(tr("控制点数量: %1").arg(mGcpTable->rowCount()));
}

void GeometricDialog::onImportGcp()
{
    QMessageBox::information(this, tr("提示"),
        tr("GCP 导入功能正在开发中。\n支持的格式: .pts, .csv"));
}

void GeometricDialog::onExportGcp()
{
    QMessageBox::information(this, tr("提示"),
        tr("GCP 导出功能正在开发中。"));
}

void GeometricDialog::onAutoDetect()
{
    if (mInput.sourceImage.isEmpty() || mInput.referenceImage.isEmpty())
    {
        QMessageBox::information(this, tr("提示"),
            tr("请先在\"输入数据\"页中选择待校正影像和参考影像。"));
        return;
    }

    // 构建参数
    GeometricCorrectionParams params;
    params.sourceImage   = mInput.sourceImage;
    params.referenceImage = mInput.referenceImage;
    params.matching.method           = mInput.matchingAlgorithm;
    params.matching.ratioThreshold   = mInput.ratioThreshold;
    params.matching.ransacThreshold  = mInput.ransacThreshold;
    params.matching.maxFeatures      = mInput.maxFeatures;

    int total = 0, inliers = 0;
    QVector<Gcp> gcps = GcpMatcher::autoMatch(
        mInput.sourceImage, mInput.referenceImage,
        params.matching, &total, &inliers);

    // 填入表格
    mGcpTable->setRowCount(0);
    mInput.gcps.clear();
    for (int i = 0; i < gcps.size(); ++i)
    {
        int row = mGcpTable->rowCount();
        mGcpTable->insertRow(row);
        mGcpTable->setItem(row, 0, new QTableWidgetItem(QString::number(i + 1)));
        mGcpTable->setItem(row, 1, new QTableWidgetItem(QString::number(gcps[i].srcX, 'f', 1)));
        mGcpTable->setItem(row, 2, new QTableWidgetItem(QString::number(gcps[i].srcY, 'f', 1)));
        mGcpTable->setItem(row, 3, new QTableWidgetItem(QString::number(gcps[i].refX, 'f', 1)));
        mGcpTable->setItem(row, 4, new QTableWidgetItem(QString::number(gcps[i].refY, 'f', 1)));
        mGcpTable->setItem(row, 5, new QTableWidgetItem(QString::number(gcps[i].residual, 'f', 3)));

        GcpEntry e;
        e.id = i + 1;
        e.srcX = gcps[i].srcX; e.srcY = gcps[i].srcY;
        e.refX = gcps[i].refX; e.refY = gcps[i].refY;
        e.residual = gcps[i].residual;
        mInput.gcps.append(e);
    }

    mLblGcpCount->setText(tr("控制点数量: %1（内点: %2 / 总计: %3）")
        .arg(inliers).arg(inliers).arg(total));

    if (inliers >= 3)
    {
        // 快速计算初步 RMSE
        GcpModel model;
        if (mInput.modelType == "TPS")
            model = GcpModelSolver::fitTPS(gcps);
        else
            model = GcpModelSolver::fitPolynomial(gcps, mInput.polynomialOrder);

        QVector<Gcp> rmsGcps = gcps;
        double rmse = GcpModelSolver::computeRMSE(rmsGcps, model);
        mLblRms->setText(tr("总体 RMSE: %1 px").arg(rmse, 0, 'f', 3));
    }
    else
    {
        mLblRms->setText(tr("总体 RMSE: N/A（控制点不足）"));
    }
}

void GeometricDialog::onAccepted()
{
    // Validate
    if (mInput.sourceImage.isEmpty())
    {
        QMessageBox::warning(this, tr("警告"), tr("请选择待校正影像。"));
        return;
    }

    // 同步 UI → mInput
    mInput.sourceImage      = mSrcPath->text();
    mInput.referenceImage   = mRefPath->text();
    mInput.outputPath       = mOutputPath->text();
    mInput.modelType        = mModelCombo->currentText();
    mInput.polynomialOrder  = mPolyOrderSpin->value();
    mInput.resampleMethod   = mResampleCombo->currentText();
    mInput.outputProjection = mOutProj->text();
    mInput.outputPixelSizeX = mPxSizeX->value();
    mInput.outputPixelSizeY = mPxSizeY->value();
    mInput.outputExtent[0]  = mExtMinX->value();
    mInput.outputExtent[1]  = mExtMinY->value();
    mInput.outputExtent[2]  = mExtMaxX->value();
    mInput.outputExtent[3]  = mExtMaxY->value();

    accept();
}
