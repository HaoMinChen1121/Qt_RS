#include "VectorReprojectionDialog.h"
#include "CrsSelectDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <gdal_priv.h>
#include <ogr_api.h>

VectorReprojectionDialog::VectorReprojectionDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QString::fromUtf8("\xe7\x9f\xa2\xe9\x87\x8f\xe9\x87\x8d\xe6\x8a\x95\xe5\xbd\xb1"));
    setMinimumSize(500, 300);
    setupUI();
}

void VectorReprojectionDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* srcG = new QGroupBox(QString::fromUtf8("\xe8\xbe\x93\xe5\x85\xa5\xe7\x9f\xa2\xe9\x87\x8f"), this);
    auto* srcL = new QHBoxLayout(srcG);
    mSrcPath = new QLineEdit(this); mSrcPath->setReadOnly(true);
    mSrcPath->setPlaceholderText(QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe7\x9f\xa2\xe9\x87\x8f\xe6\x96\x87\xe4\xbb\xb6..."));
    srcL->addWidget(mSrcPath);
    auto* btnSrc = new QPushButton(QString::fromUtf8("\xe6\xb5\x8f\xe8\xa7\x88..."), this);
    srcL->addWidget(btnSrc);
    mainLayout->addWidget(srcG);
    mSrcInfo = new QLabel(QString::fromUtf8("\xe6\x9c\xaa\xe9\x80\x89\xe6\x8b\xa9"), this);
    mSrcInfo->setWordWrap(true); mainLayout->addWidget(mSrcInfo);

    auto* crsG = new QGroupBox(QString::fromUtf8("\xe7\x9b\xae\xe6\xa0\x87\xe5\x9d\x90\xe6\xa0\x87\xe7\xb3\xbb"), this);
    auto* crsL = new QHBoxLayout(crsG);
    mCrsEdit = new QLineEdit(this); mCrsEdit->setReadOnly(true);
    mCrsEdit->setPlaceholderText(QString::fromUtf8("\xe7\x82\xb9\xe5\x87\xbb\xe9\x80\x89\xe6\x8b\xa9..."));
    crsL->addWidget(mCrsEdit);
    auto* btnCrs = new QPushButton(QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9..."), this);
    crsL->addWidget(btnCrs);
    mainLayout->addWidget(crsG);
    mCrsDesc = new QLabel(this); mainLayout->addWidget(mCrsDesc);

    auto* outG = new QGroupBox(QString::fromUtf8("\xe8\xbe\x93\xe5\x87\xba"), this);
    auto* outL = new QHBoxLayout(outG);
    mOutputPath = new QLineEdit(this); mOutputPath->setReadOnly(true);
    mOutputPath->setPlaceholderText(QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe8\xbe\x93\xe5\x87\xba\xe8\xb7\xaf\xe5\xbe\x84..."));
    outL->addWidget(mOutputPath);
    auto* btnOut = new QPushButton(QString::fromUtf8("\xe6\xb5\x8f\xe8\xa7\x88..."), this);
    outL->addWidget(btnOut);
    mainLayout->addWidget(outG);
    mainLayout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttons);

    connect(btnSrc, &QPushButton::clicked, this, &VectorReprojectionDialog::onSelectSource);
    connect(btnCrs, &QPushButton::clicked, this, &VectorReprojectionDialog::onSelectCrs);
    connect(btnOut, &QPushButton::clicked, this, &VectorReprojectionDialog::onSelectOutput);
    connect(buttons, &QDialogButtonBox::accepted, this, &VectorReprojectionDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void VectorReprojectionDialog::onSelectSource()
{
    QString path = QFileDialog::getOpenFileName(this,
        QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe7\x9f\xa2\xe9\x87\x8f"),
        QString(), QString::fromUtf8("\xe7\x9f\xa2\xe9\x87\x8f (*.shp *.geojson *.gpkg *.kml *.gml *.tab);;\xe6\x89\x80\xe6\x9c\x89 (*)"));
    if (path.isEmpty()) return;
    mSrcPath->setText(path); mParams.sourcePath = path;
    auto* ds = GDALOpenEx(path.toUtf8().constData(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
    if (ds) {
        int layers = GDALDatasetGetLayerCount(ds);
        auto* layer = GDALDatasetGetLayer(ds, 0);
        const char* proj = layer ? OGR_L_GetSpatialRef(layer) ? nullptr : nullptr : nullptr;
        OGRSpatialReferenceH srs = layer ? OGR_L_GetSpatialRef(layer) : nullptr;
        QString crsStr = QStringLiteral("Unknown");
        if (srs) {
            const char* auth = OSRGetAuthorityCode(srs, nullptr);
            if (auth) crsStr = QStringLiteral("EPSG:%1").arg(auth);
        }
        mSrcInfo->setText(QString::fromUtf8("\xe5\x9b\xbe\xe5\xb1\x82: %1  |  \xe5\x8e\x9f\xe5\x9d\x90\xe6\xa0\x87\xe7\xb3\xbb: %2")
            .arg(layers).arg(crsStr));
        GDALClose(ds);
    }
}

void VectorReprojectionDialog::onSelectCrs()
{
    bool ok = false;
    QgsCoordinateReferenceSystem crs = CrsSelectDialog::selectCrs(this, mCrs, &ok);
    if (!ok || !crs.isValid()) return;
    mCrs = crs;
    mCrsEdit->setText(crs.authid()); mCrsDesc->setText(crs.description());
    mParams.targetCrsWkt = crs.toWkt(); mParams.targetCrsAuthId = crs.authid();
}

void VectorReprojectionDialog::onSelectOutput()
{
    QString path = QFileDialog::getSaveFileName(this,
        QString::fromUtf8("\xe4\xbf\x9d\xe5\xad\x98\xe7\xbb\x93\xe6\x9e\x9c"),
        QString(), QString::fromUtf8("Shapefile (*.shp);;GeoPackage (*.gpkg);;GeoJSON (*.geojson);;\xe6\x89\x80\xe6\x9c\x89 (*)"));
    if (path.isEmpty()) return;
    mOutputPath->setText(path); mParams.outputPath = path;
}

void VectorReprojectionDialog::onAccepted()
{
    if (mParams.sourcePath.isEmpty() || !mCrs.isValid() || mParams.outputPath.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("\xe9\xaa\x8c\xe8\xaf\x81"),
            QString::fromUtf8("\xe8\xaf\xb7\xe5\xa1\xab\xe5\x86\x99\xe6\x89\x80\xe6\x9c\x89\xe5\xbf\x85\xe5\xa1\xab\xe5\xad\x97\xe6\xae\xb5"));
        return;
    }
    accept();
}

void VectorReprojectionDialog::setParams(const VectorReprojectionParams& p)
{
    mParams = p; mSrcPath->setText(p.sourcePath); mOutputPath->setText(p.outputPath);
    if (!p.targetCrsAuthId.isEmpty()) {
        mCrs = QgsCoordinateReferenceSystem(p.targetCrsWkt);
        mCrsEdit->setText(p.targetCrsAuthId); mCrsDesc->setText(mCrs.description());
    }
}

VectorReprojectionParams VectorReprojectionDialog::params() const { return mParams; }
