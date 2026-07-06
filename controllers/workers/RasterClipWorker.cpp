#include "RasterClipWorker.h"

#include <gdal_priv.h>
#include <gdal_utils.h>
#include <QFileInfo>
#include <QDir>

RasterClipWorker::RasterClipWorker(const RasterClipParams& params, QObject* parent)
    : TaskWorker(parent), mParams(params)
{
}

void RasterClipWorker::process()
{
    auto progressFn = [this](int pct, const QString& msg) -> bool {
        if (isCancelled()) {
            emit errorOccurred(tr("Cancelled by user."));
            return false;
        }
        emit progressChanged(pct, msg);
        return true;
    };

    if (mParams.rasterPath.isEmpty() || mParams.vectorPath.isEmpty() ||
        mParams.outputPath.isEmpty()) {
        emit errorOccurred(tr("Missing required parameters."));
        emit finished(false, QString());
        return;
    }

    QDir().mkpath(QFileInfo(mParams.outputPath).absolutePath());

    // ── Open raster ──
    progressFn(5, tr("Opening raster..."));
    GDALDatasetH hSrcDS = GDALOpen(mParams.rasterPath.toUtf8().constData(), GA_ReadOnly);
    if (!hSrcDS) {
        emit errorOccurred(tr("Failed to open raster."));
        emit finished(false, QString());
        return;
    }

    // ── Execute warp using command-line style arguments ──
    progressFn(15, tr("Executing clip..."));

    QByteArray cutArg = mParams.vectorPath.toUtf8();
    QByteArray outArg = mParams.outputPath.toUtf8();
    QByteArray rasterArg = mParams.rasterPath.toUtf8();

    // Build options array matching: gdalwarp -cutline vector -crop_to_cutline src dst
    char** args = nullptr;
    args = CSLAddString(args, "-cutline");
    args = CSLAddString(args, cutArg.constData());
    if (!mParams.vectorLayerName.isEmpty()) {
        args = CSLAddString(args, "-cl");
        args = CSLAddString(args, mParams.vectorLayerName.toUtf8().constData());
    }
    if (mParams.cropToCutline)
        args = CSLAddString(args, "-crop_to_cutline");
    args = CSLAddString(args, "-r");
    args = CSLAddString(args, "near");
    args = CSLAddString(args, "-multi");
    args = CSLAddString(args, "-overwrite");
    args = CSLAddString(args, "-of");
    QFileInfo fi(mParams.outputPath);
    QString ext = fi.suffix().toLower();
    if (ext == "tif" || ext == "tiff")
        args = CSLAddString(args, "GTiff");
    else
        args = CSLAddString(args, "GTiff");

    GDALWarpAppOptions* psOptions = GDALWarpAppOptionsNew(args, nullptr);
    CSLDestroy(args);

    GDALWarpAppOptionsSetProgress(psOptions, [](double dfComplete, const char*, void* p) -> int {
        auto* self = static_cast<RasterClipWorker*>(p);
        int pct = static_cast<int>(15 + dfComplete * 80);
        if (self->isCancelled()) return 0;
        emit self->progressChanged(pct, self->tr("Clipping..."));
        return 1;
    }, this);

    GDALDatasetH hOutDS = GDALWarp(outArg.constData(), nullptr,
                                    1, &hSrcDS, psOptions, nullptr);
    GDALWarpAppOptionsFree(psOptions);

    if (!hOutDS) {
        const char* gdalErr = CPLGetLastErrorMsg();
        GDALClose(hSrcDS);
        emit errorOccurred(tr("GDALWarp failed: %1").arg(QString::fromUtf8(gdalErr)));
        emit finished(false, QString());
        return;
    }

    GDALClose(hOutDS);
    GDALClose(hSrcDS);

    progressFn(100, tr("Clip complete."));
    emit finished(true, mParams.outputPath);
}
