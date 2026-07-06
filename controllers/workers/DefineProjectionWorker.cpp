#include "DefineProjectionWorker.h"
#include <gdal_priv.h>
#include <ogr_srs_api.h>
#include <QFileInfo>
#include <QDir>

DefineProjectionWorker::DefineProjectionWorker(const DefineProjectionParams& p, QObject* parent)
    : TaskWorker(parent), mParams(p) {}

void DefineProjectionWorker::process()
{
    if (mParams.sourcePath.isEmpty() || mParams.targetCrsWkt.isEmpty()) {
        emit errorOccurred(tr("Missing parameters.")); emit finished(false, {}); return;
    }

    emit progressChanged(10, tr("Opening dataset..."));
    GDALDatasetH hDS = GDALOpen(mParams.sourcePath.toUtf8().constData(), GA_Update);
    if (!hDS) {
        emit errorOccurred(tr("Cannot open file for writing. Ensure the file is not read-only."));
        emit finished(false, {});
        return;
    }

    emit progressChanged(40, tr("Setting projection..."));
    QByteArray wkt = mParams.targetCrsWkt.toUtf8();
    if (GDALSetProjection(hDS, wkt.constData()) != CE_None) {
        GDALClose(hDS);
        emit errorOccurred(tr("Failed to set projection."));
        emit finished(false, {}); return;
    }

    GDALClose(hDS);
    emit progressChanged(100, tr("Projection defined."));
    emit finished(true, mParams.sourcePath);
}
