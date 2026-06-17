#ifndef IRASTERREADER_H
#define IRASTERREADER_H

#include <QString>
#include <QVector>
#include <QSize>
#include "domain/RasterImage.h"

class IRasterReader
{
public:
    virtual ~IRasterReader() = default;

    virtual bool open(const QString& filePath) = 0;
    virtual void close() = 0;
    virtual QVector<float> readBand(int bandIndex) const = 0;
    virtual QVector<float> readBandWindow(int bandIndex, int xOff, int yOff, int xSize, int ySize) const = 0;
    virtual int bandCount() const = 0;
    virtual QSize rasterSize() const = 0;
    virtual QVector<double> geoTransform() const = 0;
    virtual QString projectionWkt() const = 0;
    virtual int epsgCode() const = 0;
    virtual QString dataType() const = 0;
    virtual double noDataValue() const = 0;
    virtual RasterImage toRasterImage(const QString& layerId, const QString& displayName) const = 0;
};

#endif // IRASTERREADER_H
