#ifndef ISENSORPRODUCT_H
#define ISENSORPRODUCT_H

#include <QString>
#include <QList>
#include <QSize>
#include "domain/SensorInfo.h"

struct RasterBandDescriptor
{
    int     physicalBand = -1;     // 传感器物理波段编号 (B1=1, B8=8, B8A=8)
    QString bandName;              // "Blue", "NIR", "SWIR-1" ...
    QString rasterPath;            // 可直接传给 GDALOpen 的路径 (/vsizip/... 或普通路径)
    double  resolution = 0.0;      // 空间分辨率 (m)
    QSize   rasterSize;            // 此波段的像素尺寸
    QString dataType;              // GDAL 数据类型名称
};

class ISensorProduct
{
public:
    virtual ~ISensorProduct() = default;

    virtual bool open(const QString& path) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual QList<RasterBandDescriptor> bands() const = 0;
    virtual QList<RasterBandDescriptor> bandsAtResolution(double res) const = 0;

    virtual SensorInfo sensorInfo() const = 0;
    virtual QString sensorType() const = 0;
    virtual QString productId() const = 0;

    virtual QString previewImagePath() const = 0;
    virtual QString originalPath() const = 0;
};

#endif // ISENSORPRODUCT_H
