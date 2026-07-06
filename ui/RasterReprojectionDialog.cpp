#include "RasterReprojectionDialog.h"
#include "CrsSelectDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <gdal_priv.h>

RasterReprojectionDialog::RasterReprojectionDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QString::fromUtf8("\xe6\xa0\x85\xe6\xa0\xbc\xe9\x87\x8d\xe6\x8a\x95\xe5\xbd\xb1"));
    setMinimumSize(520, 350);
    setupUI();
}

void RasterReprojectionDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* srcG = new QGroupBox(QString::fromUtf8("\xe8\xbe\x93\xe5\x85\xa5\xe6\xa0\x85\xe6\xa0\xbc"), this);
    auto* srcL = new QHBoxLayout(srcG);
    mSrcPath = new QLineEdit(this); mSrcPath->setReadOnly(true);
    mSrcPath->setPlaceholderText(QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe6\xa0\x85\xe6\xa0\xbc\xe6\x96\x87\xe4\xbb\xb6..."));
    srcL->addWidget(mSrcPath);
    auto* btnSrc = new QPushButton(QString::fromUtf8("\xe6\xb5\x8f\xe8\xa7\x88..."), this);
    srcL->addWidget(btnSrc);
    mainLayout->addWidget(srcG);
    mSrcInfo = new QLabel(QString::fromUtf8("\xe6\x9c\xaa\xe9\x80\x89\xe6\x8b\xa9"), this);
    mSrcInfo->setWordWrap(true); mainLayout->addWidget(mSrcInfo);

    auto* crsG = new QGroupBox(QString::fromUtf8("\xe7\x9b\xae\xe6\xa0\x87\xe5\x9d\x90\xe6\xa0\x87\xe7\xb3\xbb"), this);
    auto* crsL = new QHBoxLayout(crsG);
    mCrsEdit = new QLineEdit(this); mCrsEdit->setReadOnly(true);
    mCrsEdit->setPlaceholderText(QString::fromUtf8("\xe7\x82\xb9\xe5\x87\xbb\xe9\x80\x89\xe6\x8b\xa9\xe7\x9b\xae\xe6\xa0\x87\xe5\x9d\x90\xe6\xa0\x87\xe7\xb3\xbb..."));
    crsL->addWidget(mCrsEdit);
    auto* btnCrs = new QPushButton(QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9..."), this);
    crsL->addWidget(btnCrs);
    mainLayout->addWidget(crsG);
    mCrsDesc = new QLabel(this); mainLayout->addWidget(mCrsDesc);

    auto* form = new QFormLayout();
    mResampleCombo = new QComboBox(this);
    mResampleCombo->addItems({"near", "bilinear", "cubic", "cubicspline", "lanczos"});
    mResampleCombo->setCurrentText("near");
    form->addRow(QString::fromUtf8("\xe9\x87\x8d\xe9\x87\x87\xe6\xa0\xb7:"), mResampleCombo);
    mainLayout->addLayout(form);

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

    connect(btnSrc, &QPushButton::clicked, this, &RasterReprojectionDialog::onSelectSource);
    connect(btnCrs, &QPushButton::clicked, this, &RasterReprojectionDialog::onSelectCrs);
    connect(btnOut, &QPushButton::clicked, this, &RasterReprojectionDialog::onSelectOutput);
    connect(buttons, &QDialogButtonBox::accepted, this, &RasterReprojectionDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void RasterReprojectionDialog::onSelectSource()
{
    QString path = QFileDialog::getOpenFileName(this,
        QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe6\xa0\x85\xe6\xa0\xbc"),
        QString(), QString::fromUtf8("\xe6\xa0\x85\xe6\xa0\xbc (*.tif *.tiff *.img *.jp2 *.png);;\xe6\x89\x80\xe6\x9c\x89 (*)"));
    if (path.isEmpty()) return;
    mSrcPath->setText(path); mParams.sourcePath = path;
    auto* ds = GDALOpen(path.toUtf8().constData(), GA_ReadOnly);
    if (ds) {
        int w = GDALGetRasterXSize(ds), h = GDALGetRasterYSize(ds);
        const char* proj = GDALGetProjectionRef(ds);
        QgsCoordinateReferenceSystem srcCrs(QString::fromUtf8(proj));
        mSrcInfo->setText(QString::fromUtf8("\xe5\x83\x8f\xe7\xb4\xa0: %1x%2  |  \xe5\x8e\x9f\xe5\x9d\x90\xe6\xa0\x87\xe7\xb3\xbb: %3")
            .arg(w).arg(h).arg(srcCrs.isValid() ? srcCrs.authid() : QStringLiteral("Unknown")));
        GDALClose(ds);
    }
}

void RasterReprojectionDialog::onSelectCrs()
{
    bool ok = false;
    QgsCoordinateReferenceSystem crs = CrsSelectDialog::selectCrs(this, mCrs, &ok);
    if (!ok || !crs.isValid()) return;
    mCrs = crs;
    mCrsEdit->setText(crs.authid());
    mCrsDesc->setText(crs.description());
    mParams.targetCrsWkt = crs.toWkt();
    mParams.targetCrsAuthId = crs.authid();
}

void RasterReprojectionDialog::onSelectOutput()
{
    QString path = QFileDialog::getSaveFileName(this,
        QString::fromUtf8("\xe4\xbf\x9d\xe5\xad\x98\xe7\xbb\x93\xe6\x9e\x9c"),
        QString(), QString::fromUtf8("GeoTIFF (*.tif *.tiff);;\xe6\x89\x80\xe6\x9c\x89 (*)"));
    if (path.isEmpty()) return;
    mOutputPath->setText(path); mParams.outputPath = path;
}

void RasterReprojectionDialog::onAccepted()
{
    if (mParams.sourcePath.isEmpty() || !mCrs.isValid() || mParams.outputPath.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("\xe9\xaa\x8c\xe8\xaf\x81"),
            QString::fromUtf8("\xe8\xaf\xb7\xe5\xa1\xab\xe5\x86\x99\xe6\x89\x80\xe6\x9c\x89\xe5\xbf\x85\xe5\xa1\xab\xe5\xad\x97\xe6\xae\xb5"));
        return;
    }
    mParams.resampleMethod = mResampleCombo->currentText();
    accept();
}

void RasterReprojectionDialog::setParams(const RasterReprojectionParams& p)
{
    mParams = p; mSrcPath->setText(p.sourcePath); mOutputPath->setText(p.outputPath);
    mResampleCombo->setCurrentText(p.resampleMethod);
    if (!p.targetCrsAuthId.isEmpty()) {
        mCrs = QgsCoordinateReferenceSystem(p.targetCrsWkt);
        mCrsEdit->setText(p.targetCrsAuthId); mCrsDesc->setText(mCrs.description());
    }
}

RasterReprojectionParams RasterReprojectionDialog::params() const { return mParams; }
