#ifndef RASTERIMAGE_H
#define RASTERIMAGE_H

#include <QString>
#include <QSize>
#include <QRectF>
#include <QVector>
#include <QMetaType>

/**
 * @brief 已加载栅格图层的值对象
 *
 * 不持有像素数据，仅存储元信息和定位参数。
 * 由 ILayerService 在加载图层时创建。
 */
struct RasterImage
{
    QString layerId;
    QString displayName;
    QString filePath;           // 用户选择的原始路径 (如 .zip 文件路径)
    QString rasterSourcePath;   // GDAL/QGIS 实际打开的路径 (可能是 /vsizip/...)
    QString productId;          // 所属产品的标识 (同一个产品的多个波段共享)
    int     physicalBand = 0;   // 传感器物理波段号 (如 B4 对应 4)
    QString sensorType;
    int bandCount = 0;
    QSize rasterSize;
    QVector<double> geoTransform; // 6 elements: originX, pixelWidth, rotationX, originY, rotationY, pixelHeight
    QString projectionWkt;
    int epsgCode = -1;
    QString dataType;
    double noDataValue = 0.0;
    bool visible = true;
    double opacity = 1.0;

    QRectF extent() const
    {
        if (geoTransform.size() < 6 || rasterSize.isEmpty())
            return {};
        double minX = geoTransform[0];
        double maxY = geoTransform[3];
        double maxX = minX + rasterSize.width() * geoTransform[1];
        double minY = maxY + rasterSize.height() * geoTransform[5];
        return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
    }
};

Q_DECLARE_METATYPE(RasterImage)

#endif // RASTERIMAGE_H
