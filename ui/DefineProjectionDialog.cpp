#include "DefineProjectionDialog.h"
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

DefineProjectionDialog::DefineProjectionDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QString::fromUtf8("\xe5\xae\x9a\xe4\xb9\x89\xe6\x8a\x95\xe5\xbd\xb1"));
    setMinimumSize(500, 260);
    setupUI();
}

void DefineProjectionDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* srcG = new QGroupBox(QString::fromUtf8("\xe8\xbe\x93\xe5\x85\xa5\xe6\x96\x87\xe4\xbb\xb6"), this);
    auto* srcL = new QHBoxLayout(srcG);
    mSrcPath = new QLineEdit(this); mSrcPath->setReadOnly(true);
    mSrcPath->setPlaceholderText(QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe6\xa0\x85\xe6\xa0\xbc\xe6\x88\x96\xe7\x9f\xa2\xe9\x87\x8f\xe6\x96\x87\xe4\xbb\xb6..."));
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
    mainLayout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttons);

    connect(btnSrc, &QPushButton::clicked, this, &DefineProjectionDialog::onSelectSource);
    connect(btnCrs, &QPushButton::clicked, this, &DefineProjectionDialog::onSelectCrs);
    connect(buttons, &QDialogButtonBox::accepted, this, &DefineProjectionDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void DefineProjectionDialog::onSelectSource()
{
    QString path = QFileDialog::getOpenFileName(this,
        QString::fromUtf8("\xe9\x80\x89\xe6\x8b\xa9\xe6\x96\x87\xe4\xbb\xb6"),
        QString(), QString::fromUtf8("GDAL\xe6\x94\xaf\xe6\x8c\x81\xe7\x9a\x84\xe6\xa0\xbc\xe5\xbc\x8f (*.tif *.tiff *.img *.shp *.gpkg *.geojson);;\xe6\x89\x80\xe6\x9c\x89 (*)"));
    if (path.isEmpty()) return;
    mSrcPath->setText(path); mParams.sourcePath = path;
    auto* ds = GDALOpen(path.toUtf8().constData(), GA_ReadOnly);
    if (ds) {
        const char* proj = GDALGetProjectionRef(ds);
        mSrcInfo->setText(QString::fromUtf8("\xe5\xbd\x93\xe5\x89\x8d\xe6\x8a\x95\xe5\xbd\xb1: %1")
            .arg(proj && strlen(proj) > 0 ? QString::fromUtf8(proj).left(60) : QStringLiteral("None")));
        GDALClose(ds);
    }
}

void DefineProjectionDialog::onSelectCrs()
{
    bool ok = false;
    QgsCoordinateReferenceSystem crs = CrsSelectDialog::selectCrs(this, mCrs, &ok);
    if (!ok || !crs.isValid()) return;
    mCrs = crs;
    mCrsEdit->setText(crs.authid()); mCrsDesc->setText(crs.description());
    mParams.targetCrsWkt = crs.toWkt(); mParams.targetCrsAuthId = crs.authid();
}

void DefineProjectionDialog::onAccepted()
{
    if (mParams.sourcePath.isEmpty() || !mCrs.isValid()) {
        QMessageBox::warning(this, QString::fromUtf8("\xe9\xaa\x8c\xe8\xaf\x81"),
            QString::fromUtf8("\xe8\xaf\xb7\xe9\x80\x89\xe6\x8b\xa9\xe6\x96\x87\xe4\xbb\xb6\xe5\x92\x8c\xe7\x9b\xae\xe6\xa0\x87\xe5\x9d\x90\xe6\xa0\x87\xe7\xb3\xbb"));
        return;
    }
    accept();
}

void DefineProjectionDialog::setParams(const DefineProjectionParams& p)
{
    mParams = p; mSrcPath->setText(p.sourcePath);
    if (!p.targetCrsAuthId.isEmpty()) {
        mCrs = QgsCoordinateReferenceSystem(p.targetCrsWkt);
        mCrsEdit->setText(p.targetCrsAuthId); mCrsDesc->setText(mCrs.description());
    }
}

DefineProjectionParams DefineProjectionDialog::params() const { return mParams; }
