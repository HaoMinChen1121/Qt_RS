#include "RasterClipDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>

#include <gdal_priv.h>
#include <ogr_api.h>

RasterClipDialog::RasterClipDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString::fromUtf8("\xe6\xa0\x85\xe6\xa0\xbc\xe8\xa3\x81\xe5\x89\xaa"));
    setMinimumSize(520, 400);
    setupUI();
}

void RasterClipDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // ── Input raster ──
    auto* rasterGroup = new QGroupBox(
        QString::fromUtf8("\xe8\xbe\x93\xe5\x85\xa5\xe6\xa0\x85\xe6\xa0\xbc"), this);
    auto* rasterLayout = new QHBoxLayout(rasterGroup);
    mRasterPath = new QLineEdit(this);
    mRasterPath->setReadOnly(true);
    mRasterPath->setPlaceholderText(
        QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe6\xa0\x85\xe6\xa0\xbc\xe6\x96\x87\xe4\xbb\xb6..."));
    rasterLayout->addWidget(mRasterPath);
    auto* btnRaster = new QPushButton(
        QString::fromUtf8("\xe6\xb5\x8f\xe8\xa7\x88..."), this);
    rasterLayout->addWidget(btnRaster);
    mainLayout->addWidget(rasterGroup);

    mRasterInfo = new QLabel(
        QString::fromUtf8("\xe6\x9c\xaa\xe9\x80\x89\xe6\x8b\xa9\xe6\x96\x87\xe4\xbb\xb6"), this);
    mRasterInfo->setWordWrap(true);
    mainLayout->addWidget(mRasterInfo);

    // ── Clip boundary (vector) ──
    auto* vecGroup = new QGroupBox(
        QString::fromUtf8("\xe8\xa3\x81\xe5\x89\xaa\xe8\xbe\xb9\xe7\x95\x8c (\xe7\x9f\xa2\xe9\x87\x8f)"), this);
    auto* vecLayout = new QHBoxLayout(vecGroup);
    mVectorPath = new QLineEdit(this);
    mVectorPath->setReadOnly(true);
    mVectorPath->setPlaceholderText(
        QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe7\x9f\xa2\xe9\x87\x8f\xe6\x96\x87\xe4\xbb\xb6..."));
    vecLayout->addWidget(mVectorPath);
    auto* btnVector = new QPushButton(
        QString::fromUtf8("\xe6\xb5\x8f\xe8\xa7\x88..."), this);
    vecLayout->addWidget(btnVector);
    mainLayout->addWidget(vecGroup);

    mVectorInfo = new QLabel(
        QString::fromUtf8("\xe6\x9c\xaa\xe9\x80\x89\xe6\x8b\xa9\xe6\x96\x87\xe4\xbb\xb6"), this);
    mVectorInfo->setWordWrap(true);
    mainLayout->addWidget(mVectorInfo);

    // ── Layer selection ──
    auto* layerRow = new QHBoxLayout();
    auto* layerLabel = new QLabel(
        QString::fromUtf8("\xe5\x9b\xbe\xe5\xb1\x82:"), this);
    mLayerCombo = new QComboBox(this);
    mLayerCombo->setMinimumWidth(200);
    layerRow->addWidget(layerLabel);
    layerRow->addWidget(mLayerCombo);
    layerRow->addStretch();
    mainLayout->addLayout(layerRow);

    // ── Options ──
    mCropCheck = new QCheckBox(
        QString::fromUtf8("\xe6\x8c\x89\xe8\xbe\xb9\xe7\x95\x8c\xe8\xa3\x81\xe5\x89\xaa (Crop to Cutline)"), this);
    mCropCheck->setChecked(true);
    mainLayout->addWidget(mCropCheck);

    // ── Output ──
    auto* outGroup = new QGroupBox(
        QString::fromUtf8("\xe8\xbe\x93\xe5\x87\xba\xe6\x96\x87\xe4\xbb\xb6"), this);
    auto* outLayout = new QHBoxLayout(outGroup);
    mOutputPath = new QLineEdit(this);
    mOutputPath->setReadOnly(true);
    mOutputPath->setPlaceholderText(
        QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe8\xbe\x93\xe5\x87\xba\xe8\xb7\xaf\xe5\xbe\x84..."));
    outLayout->addWidget(mOutputPath);
    auto* btnOut = new QPushButton(
        QString::fromUtf8("\xe6\xb5\x8f\xe8\xa7\x88..."), this);
    outLayout->addWidget(btnOut);
    mainLayout->addWidget(outGroup);

    mainLayout->addStretch();

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(btnRaster, &QPushButton::clicked, this, &RasterClipDialog::onSelectRaster);
    connect(btnVector, &QPushButton::clicked, this, &RasterClipDialog::onSelectVector);
    connect(btnOut, &QPushButton::clicked, this, &RasterClipDialog::onSelectOutput);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RasterClipDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void RasterClipDialog::refreshLayerList()
{
    mLayerCombo->clear();
    if (mParams.vectorPath.isEmpty()) return;

    auto* ds = GDALOpenEx(mParams.vectorPath.toUtf8().constData(),
                           GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
    if (!ds) return;

    int count = GDALDatasetGetLayerCount(ds);
    for (int i = 0; i < count; ++i) {
        auto* layer = GDALDatasetGetLayer(ds, i);
        if (layer)
            mLayerCombo->addItem(QString::fromUtf8(OGR_L_GetName(layer)));
    }
    GDALClose(ds);

    if (mLayerCombo->count() > 0)
        mLayerCombo->setCurrentIndex(0);
}

void RasterClipDialog::onSelectRaster()
{
    QString path = QFileDialog::getOpenFileName(this,
        QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe6\xa0\x85\xe6\xa0\xbc\xe6\x96\x87\xe4\xbb\xb6"),
        QString(),
        QString::fromUtf8("\xe6\xa0\x85\xe6\xa0\xbc\xe6\x96\x87\xe4\xbb\xb6 (*.tif *.tiff *.img *.jp2 *.png);;"
                          "\xe6\x89\x80\xe6\x9c\x89\xe6\x96\x87\xe4\xbb\xb6 (*)"));
    if (path.isEmpty()) return;
    mRasterPath->setText(path);
    mParams.rasterPath = path;

    auto* ds = GDALOpen(path.toUtf8().constData(), GA_ReadOnly);
    if (ds) {
        int w = GDALGetRasterXSize(ds);
        int h = GDALGetRasterYSize(ds);
        int bands = GDALGetRasterCount(ds);
        double gt[6];
        GDALGetGeoTransform(ds, gt);
        const char* proj = GDALGetProjectionRef(ds);
        mRasterInfo->setText(QString::fromUtf8(
            "\xe5\x83\x8f\xe7\xb4\xa0: %1 x %2  |  \xe6\xb3\xa2\xe6\xae\xb5: %3  |  "
            "\xe5\x83\x8f\xe5\x85\x83\xe5\xa4\xa7\xe5\xb0\x8f: %4 x %5")
            .arg(w).arg(h).arg(bands)
            .arg(gt[1], 0, 'f', 6).arg(std::abs(gt[5]), 0, 'f', 6));
        GDALClose(ds);
    }
}

void RasterClipDialog::onSelectVector()
{
    QString path = QFileDialog::getOpenFileName(this,
        QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe8\xa3\x81\xe5\x89\xaa\xe7\x9f\xa2\xe9\x87\x8f"),
        QString(),
        QString::fromUtf8("\xe7\x9f\xa2\xe9\x87\x8f\xe6\x96\x87\xe4\xbb\xb6 (*.shp *.geojson *.gpkg *.kml *.gml *.tab *.mif);;"
                          "\xe6\x89\x80\xe6\x9c\x89\xe6\x96\x87\xe4\xbb\xb6 (*)"));
    if (path.isEmpty()) return;
    mVectorPath->setText(path);
    mParams.vectorPath = path;

    auto* ds = GDALOpenEx(path.toUtf8().constData(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
    if (ds) {
        int layers = GDALDatasetGetLayerCount(ds);
        int totalFeatures = 0;
        for (int i = 0; i < layers; ++i) {
            auto* layer = GDALDatasetGetLayer(ds, i);
            if (layer)
                totalFeatures += static_cast<int>(OGR_L_GetFeatureCount(layer, 1));
        }
        mVectorInfo->setText(QString::fromUtf8(
            "\xe5\x9b\xbe\xe5\xb1\x82: %1  |  \xe8\xa6\x81\xe7\xb4\xa0: %2")
            .arg(layers).arg(totalFeatures));
        GDALClose(ds);
    }
    refreshLayerList();
}

void RasterClipDialog::onSelectOutput()
{
    QString path = QFileDialog::getSaveFileName(this,
        QString::fromUtf8("\xe4\xbf\x9d\xe5\xad\x98\xe8\xa3\x81\xe5\x89\xaa\xe7\xbb\x93\xe6\x9e\x9c"),
        mParams.rasterPath.isEmpty() ? QString() :
            mParams.rasterPath.left(mParams.rasterPath.lastIndexOf('.')) + "_clip.tif",
        QString::fromUtf8("GeoTIFF (*.tif *.tiff);;\xe6\x89\x80\xe6\x9c\x89\xe6\x96\x87\xe4\xbb\xb6 (*)"));
    if (path.isEmpty()) return;
    mOutputPath->setText(path);
    mParams.outputPath = path;
}

void RasterClipDialog::setParams(const RasterClipParams& params)
{
    mParams = params;
    mRasterPath->setText(params.rasterPath);
    mVectorPath->setText(params.vectorPath);
    mOutputPath->setText(params.outputPath);
    mCropCheck->setChecked(params.cropToCutline);
    if (!params.vectorPath.isEmpty())
        refreshLayerList();
    if (!params.vectorLayerName.isEmpty())
        mLayerCombo->setCurrentText(params.vectorLayerName);
}

RasterClipParams RasterClipDialog::params() const
{
    return mParams;
}

void RasterClipDialog::onAccepted()
{
    if (mParams.rasterPath.isEmpty()) {
        QMessageBox::warning(this,
            QString::fromUtf8("\xe9\xaa\x8c\xe8\xaf\x81"),
            QString::fromUtf8("\xe8\xaf\xb7\xe9\x80\x89\xe6\x8b\xa9\xe6\xa0\x85\xe6\xa0\xbc\xe6\x96\x87\xe4\xbb\xb6"));
        return;
    }
    if (mParams.vectorPath.isEmpty()) {
        QMessageBox::warning(this,
            QString::fromUtf8("\xe9\xaa\x8c\xe8\xaf\x81"),
            QString::fromUtf8("\xe8\xaf\xb7\xe9\x80\x89\xe6\x8b\xa9\xe8\xa3\x81\xe5\x89\xaa\xe7\x9f\xa2\xe9\x87\x8f"));
        return;
    }
    if (mParams.outputPath.isEmpty()) {
        QMessageBox::warning(this,
            QString::fromUtf8("\xe9\xaa\x8c\xe8\xaf\x81"),
            QString::fromUtf8("\xe8\xaf\xb7\xe6\x8c\x87\xe5\xae\x9a\xe8\xbe\x93\xe5\x87\xba\xe6\x96\x87\xe4\xbb\xb6\xe8\xb7\xaf\xe5\xbe\x84"));
        return;
    }

    mParams.cropToCutline = mCropCheck->isChecked();
    mParams.vectorLayerName = mLayerCombo->currentText();
    accept();
}
