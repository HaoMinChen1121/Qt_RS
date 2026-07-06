#include "RasterReprojectionWorker.h"
#include <gdal_priv.h>
#include <gdal_utils.h>
#include <QFileInfo>
#include <QDir>

RasterReprojectionWorker::RasterReprojectionWorker(const RasterReprojectionParams& p, QObject* parent)
    : TaskWorker(parent), mParams(p) {}

void RasterReprojectionWorker::process()
{
    auto progressFn = [this](int pct, const QString& msg) -> bool {
        if (isCancelled()) { emit errorOccurred(tr("Cancelled.")); return false; }
        emit progressChanged(pct, msg); return true;
    };
    if (mParams.sourcePath.isEmpty() || mParams.targetCrsWkt.isEmpty() || mParams.outputPath.isEmpty()) {
        emit errorOccurred(tr("Missing parameters.")); emit finished(false, {}); return;
    }
    QDir().mkpath(QFileInfo(mParams.outputPath).absolutePath());
    progressFn(5, tr("Opening raster..."));
    GDALDatasetH hSrc = GDALOpen(mParams.sourcePath.toUtf8().constData(), GA_ReadOnly);
    if (!hSrc) { emit errorOccurred(tr("Cannot open source.")); emit finished(false, {}); return; }

    progressFn(10, tr("Reprojecting..."));
    QByteArray srcP = mParams.sourcePath.toUtf8(), dstP = mParams.outputPath.toUtf8(),
               crsW = mParams.targetCrsWkt.toUtf8(), res = mParams.resampleMethod.toUtf8();
    char** args = nullptr;
    args = CSLAddString(args, "-t_srs");  args = CSLAddString(args, crsW.constData());
    args = CSLAddString(args, "-r");       args = CSLAddString(args, res.constData());
    args = CSLAddString(args, "-multi");
    args = CSLAddString(args, "-overwrite");
    args = CSLAddString(args, "-of");      args = CSLAddString(args, "GTiff");

    GDALWarpAppOptions* opts = GDALWarpAppOptionsNew(args, nullptr);
    CSLDestroy(args);

    GDALWarpAppOptionsSetProgress(opts, [](double df, const char*, void* p) -> int {
        auto* s = static_cast<RasterReprojectionWorker*>(p);
        if (s->isCancelled()) return 0;
        emit s->progressChanged(10 + (int)(df * 85), s->tr("Reprojecting..."));
        return 1;
    }, this);

    GDALDatasetH hOut = GDALWarp(dstP.constData(), nullptr, 1, &hSrc, opts, nullptr);
    GDALWarpAppOptionsFree(opts);

    if (!hOut) {
        const char* err = CPLGetLastErrorMsg();
        GDALClose(hSrc);
        emit errorOccurred(tr("Failed: %1").arg(QString::fromUtf8(err)));
        emit finished(false, {}); return;
    }
    GDALClose(hOut); GDALClose(hSrc);
    progressFn(100, tr("Reprojection complete."));
    emit finished(true, mParams.outputPath);
}
