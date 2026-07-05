#ifndef VECTORLAYERINFO_H
#define VECTORLAYERINFO_H

#include <QString>
#include <QRectF>
#include <QStringList>
#include <QMetaType>

struct VectorLayerInfo
{
    QString layerId;
    QString displayName;
    QString filePath;
    int featureCount = 0;
    QString geometryType;     // Point, LineString, Polygon, MultiPoint, etc.
    QString projectionWkt;
    int epsgCode = -1;
    QRectF extent;
    QStringList fieldNames;
    QStringList fieldTypes;   // Integer, Integer64, Real, String, Date, etc.
    bool visible = true;
    double opacity = 1.0;
};

Q_DECLARE_METATYPE(VectorLayerInfo)

#endif // VECTORLAYERINFO_H
