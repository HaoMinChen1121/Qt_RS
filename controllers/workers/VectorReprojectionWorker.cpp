#include "VectorReprojectionWorker.h"
#include <gdal_priv.h>
#include <gdal_utils.h>
#include <QFileInfo>
#include <QDir>

VectorReprojectionWorker::VectorReprojectionWorker(const VectorReprojectionParams& p, QObject* parent)
    : TaskWorker(parent), mParams(p) {}

void VectorReprojectionWorker::process()
{
    auto progressFn = [this](int pct, const QString& msg) -> bool {
        if (isCancelled()) { emit errorOccurred(tr("Cancelled.")); return false; }
        emit progressChanged(pct, msg); return true;
    };
    if (mParams.sourcePath.isEmpty() || mParams.targetCrsWkt.isEmpty() || mParams.outputPath.isEmpty()) {
        emit errorOccurred(tr("Missing parameters.")); emit finished(false, {}); return;
    }
    QDir().mkpath(QFileInfo(mParams.outputPath).absolutePath());

    progressFn(10, tr("Reprojecting vector..."));

    QByteArray srcP = mParams.sourcePath.toUtf8(), dstP = mParams.outputPath.toUtf8(),
               crsW = mParams.targetCrsWkt.toUtf8();

    // Use ogr2ogr via GDALVectorTranslate: -t_srs targetCrs src dst
    char** args = nullptr;
    args = CSLAddString(args, "-t_srs");     args = CSLAddString(args, crsW.constData());
    args = CSLAddString(args, "-overwrite");

    // Auto-detect output format from extension
    QFileInfo fi(mParams.outputPath);
    QString ext = fi.suffix().toLower();
    QByteArray fmt;
    if (ext == "shp")      fmt = "ESRI Shapefile";
    else if (ext == "gpkg") fmt = "GPKG";
    else if (ext == "geojson") fmt = "GeoJSON";
    else if (ext == "kml") fmt = "KML";
    else fmt = "ESRI Shapefile";
    args = CSLAddString(args, "-f"); args = CSLAddString(args, fmt.constData());

    GDALVectorTranslateOptions* opts = GDALVectorTranslateOptionsNew(args, nullptr);
    CSLDestroy(args);

    GDALVectorTranslateOptionsSetProgress(opts, [](double df, const char*, void* p) -> int {
        auto* s = static_cast<VectorReprojectionWorker*>(p);
        if (s->isCancelled()) return 0;
        emit s->progressChanged(10 + (int)(df * 85), s->tr("Reprojecting..."));
        return 1;
    }, this);

    GDALDatasetH hSrc = GDALOpenEx(srcP.constData(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr);
    if (!hSrc) {
        emit errorOccurred(tr("Cannot open source vector.")); emit finished(false, {}); return;
    }

    GDALDatasetH hOut = GDALVectorTranslate(dstP.constData(), nullptr, 1, &hSrc, opts, nullptr);
    GDALVectorTranslateOptionsFree(opts);
    GDALClose(hSrc);

    if (!hOut) {
        const char* err = CPLGetLastErrorMsg();
        emit errorOccurred(tr("ogr2ogr failed: %1").arg(QString::fromUtf8(err)));
        emit finished(false, {}); return;
    }
    GDALClose(hOut);
    progressFn(100, tr("Vector reprojection complete."));
    emit finished(true, mParams.outputPath);
}
