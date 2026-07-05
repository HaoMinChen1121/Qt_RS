#include "GdalVectorReader.h"
#include <gdal_priv.h>
#include <ogr_api.h>
#include <ogr_srs_api.h>
#include <ogrsf_frmts.h>
#include <QDebug>
#include <QSet>

GdalVectorReader::~GdalVectorReader()
{
    close();
}

bool GdalVectorReader::open(const QString& filePath)
{
    close();

    GDALAllRegister();

    const QByteArray pathBytes = filePath.toUtf8();
    GDALDataset* ds = static_cast<GDALDataset*>(
        GDALOpenEx(pathBytes.constData(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr));
    if (!ds) {
        qWarning() << "[GdalVectorReader] failed to open:" << filePath;
        return false;
    }

    mDataset = ds;
    qDebug() << "[GdalVectorReader] opened:" << filePath
             << "layers:" << ds->GetLayerCount();
    return true;
}

void GdalVectorReader::close()
{
    if (mDataset) {
        GDALClose(mDataset);
        mDataset = nullptr;
    }
}

int GdalVectorReader::layerCount() const
{
    return mDataset ? mDataset->GetLayerCount() : 0;
}

QStringList GdalVectorReader::layerNames() const
{
    QStringList names;
    if (!mDataset) return names;
    for (int i = 0; i < mDataset->GetLayerCount(); ++i) {
        OGRLayer* layer = mDataset->GetLayer(i);
        if (layer)
            names << QString::fromUtf8(layer->GetName());
    }
    return names;
}

int GdalVectorReader::featureCount(int layerIndex) const
{
    if (!mDataset) return 0;
    OGRLayer* layer = mDataset->GetLayer(layerIndex);
    if (!layer) return 0;

    GIntBig count = layer->GetFeatureCount(false);
    if (count >= 0)
        return static_cast<int>(count);

    // Force count for drivers that don't support GetFeatureCount
    int n = 0;
    layer->ResetReading();
    while (OGRFeature* feat = layer->GetNextFeature()) {
        ++n;
        OGRFeature::DestroyFeature(feat);
    }
    layer->ResetReading();
    return n;
}

QString GdalVectorReader::geometryType(int layerIndex) const
{
    if (!mDataset) return {};
    OGRLayer* layer = mDataset->GetLayer(layerIndex);
    if (!layer) return {};
    OGRwkbGeometryType gType = layer->GetGeomType();
    const char* name = OGRGeometryTypeToName(gType);
    return name ? QString::fromUtf8(name) : QStringLiteral("Unknown");
}

QString GdalVectorReader::projectionWkt() const
{
    if (!mDataset) return {};

    // Try first layer's spatial reference first
    OGRLayer* layer = mDataset->GetLayer(0);
    if (layer) {
        OGRSpatialReference* srs = layer->GetSpatialRef();
        if (srs) {
            char* wkt = nullptr;
            srs->exportToWkt(&wkt);
            QString result = QString::fromUtf8(wkt);
            CPLFree(wkt);
            return result;
        }
    }

    // Fall back to dataset-level projection
    const char* wkt = mDataset->GetProjectionRef();
    return wkt ? QString::fromUtf8(wkt) : QString();
}

int GdalVectorReader::epsgCode() const
{
    if (!mDataset) return -1;

    OGRLayer* layer = mDataset->GetLayer(0);
    if (!layer) return -1;

    OGRSpatialReference* srs = layer->GetSpatialRef();
    if (!srs) {
        // Try dataset-level
        const char* wkt = mDataset->GetProjectionRef();
        if (!wkt || wkt[0] == '\0') return -1;
        OGRSpatialReference dsSrs(wkt);
        const char* code = dsSrs.GetAuthorityCode(nullptr);
        if (code)
            return QString::fromLatin1(code).toInt();
        if (dsSrs.AutoIdentifyEPSG() == OGRERR_NONE) {
            code = dsSrs.GetAuthorityCode(nullptr);
            if (code)
                return QString::fromLatin1(code).toInt();
        }
        return -1;
    }

    const char* code = srs->GetAuthorityCode(nullptr);
    if (code)
        return QString::fromLatin1(code).toInt();

    if (srs->AutoIdentifyEPSG() == OGRERR_NONE) {
        code = srs->GetAuthorityCode(nullptr);
        if (code)
            return QString::fromLatin1(code).toInt();
    }

    return -1;
}

QRectF GdalVectorReader::extent(int layerIndex) const
{
    if (!mDataset) return {};
    OGRLayer* layer = mDataset->GetLayer(layerIndex);
    if (!layer) return {};

    OGREnvelope env;
    if (layer->GetExtent(&env) != OGRERR_NONE)
        return {};
    return QRectF(QPointF(env.MinX, env.MinY), QPointF(env.MaxX, env.MaxY));
}

QStringList GdalVectorReader::fieldNames(int layerIndex) const
{
    QStringList names;
    if (!mDataset) return names;
    OGRLayer* layer = mDataset->GetLayer(layerIndex);
    if (!layer) return names;

    OGRFeatureDefn* defn = layer->GetLayerDefn();
    if (!defn) return names;

    for (int i = 0; i < defn->GetFieldCount(); ++i) {
        OGRFieldDefn* fd = defn->GetFieldDefn(i);
        if (fd)
            names << QString::fromUtf8(fd->GetNameRef());
    }
    return names;
}

QStringList GdalVectorReader::fieldTypes(int layerIndex) const
{
    QStringList types;
    if (!mDataset) return types;
    OGRLayer* layer = mDataset->GetLayer(layerIndex);
    if (!layer) return types;

    OGRFeatureDefn* defn = layer->GetLayerDefn();
    if (!defn) return types;

    for (int i = 0; i < defn->GetFieldCount(); ++i) {
        OGRFieldDefn* fd = defn->GetFieldDefn(i);
        if (fd)
            types << QString::fromUtf8(OGRFieldDefn::GetFieldTypeName(fd->GetType()));
    }
    return types;
}

VectorLayerInfo GdalVectorReader::toVectorLayerInfo(const QString& layerId,
                                                     const QString& displayName,
                                                     int layerIndex) const
{
    VectorLayerInfo info;
    info.layerId       = layerId;
    info.displayName   = displayName;
    info.featureCount  = featureCount(layerIndex);
    info.geometryType  = geometryType(layerIndex);
    info.projectionWkt = projectionWkt();
    info.epsgCode      = epsgCode();
    info.extent        = extent(layerIndex);
    info.fieldNames    = fieldNames(layerIndex);
    info.fieldTypes    = fieldTypes(layerIndex);
    return info;
}

QStringList GdalVectorReader::uniqueValues(int layerIndex, const QString& fieldName) const
{
    QStringList values;
    if (!mDataset) return values;
    OGRLayer* layer = mDataset->GetLayer(layerIndex);
    if (!layer) return values;

    QSet<QString> seen;
    layer->ResetReading();
    while (OGRFeature* feat = layer->GetNextFeature())
    {
        int idx = feat->GetFieldIndex(fieldName.toUtf8().constData());
        if (idx >= 0 && feat->IsFieldSetAndNotNull(idx))
        {
            const QString val = QString::fromUtf8(feat->GetFieldAsString(idx));
            if (!seen.contains(val))
            {
                seen.insert(val);
                values.append(val);
            }
        }
        OGRFeature::DestroyFeature(feat);
    }
    layer->ResetReading();
    return values;
}

bool GdalVectorReader::numericFieldRange(int layerIndex, const QString& fieldName,
                                          double& minVal, double& maxVal) const
{
    if (!mDataset) return false;
    OGRLayer* layer = mDataset->GetLayer(layerIndex);
    if (!layer) return false;

    bool first = true;
    minVal = 0.0;
    maxVal = 0.0;
    layer->ResetReading();
    while (OGRFeature* feat = layer->GetNextFeature())
    {
        int idx = feat->GetFieldIndex(fieldName.toUtf8().constData());
        if (idx >= 0 && feat->IsFieldSetAndNotNull(idx))
        {
            double v = feat->GetFieldAsDouble(idx);
            if (first) { minVal = v; maxVal = v; first = false; }
            else if (v < minVal) minVal = v;
            else if (v > maxVal) maxVal = v;
        }
        OGRFeature::DestroyFeature(feat);
    }
    layer->ResetReading();
    return !first;
}
