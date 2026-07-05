#ifndef IVECTORREADER_H
#define IVECTORREADER_H

#include <QString>
#include <QStringList>
#include <QRectF>
#include "domain/VectorLayerInfo.h"

class IVectorReader
{
public:
    virtual ~IVectorReader() = default;

    virtual bool open(const QString& filePath) = 0;
    virtual void close() = 0;
    virtual int layerCount() const = 0;
    virtual QStringList layerNames() const = 0;
    virtual int featureCount(int layerIndex = 0) const = 0;
    virtual QString geometryType(int layerIndex = 0) const = 0;
    virtual QString projectionWkt() const = 0;
    virtual int epsgCode() const = 0;
    virtual QRectF extent(int layerIndex = 0) const = 0;
    virtual QStringList fieldNames(int layerIndex = 0) const = 0;
    virtual QStringList fieldTypes(int layerIndex = 0) const = 0;

    virtual VectorLayerInfo toVectorLayerInfo(const QString& layerId,
                                              const QString& displayName,
                                              int layerIndex = 0) const = 0;

    // 字段查询 (用于分类/渐变着色)
    virtual QStringList uniqueValues(int layerIndex, const QString& fieldName) const = 0;
    virtual bool numericFieldRange(int layerIndex, const QString& fieldName,
                                   double& minVal, double& maxVal) const = 0;
};

#endif // IVECTORREADER_H
