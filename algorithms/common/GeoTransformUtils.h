#ifndef GEOTRANSFORMUTILS_H
#define GEOTRANSFORMUTILS_H

#include <QVector>
#include <QPair>

struct GcpPoint;

/**
 * @brief GDAL 地理变换辅助工具函数集
 *
 * 提供像素坐标 ↔ 地理坐标互转、两幅影像重叠区域计算等功能。
 * 所有函数均基于 GDAL 六参数地理变换模型（GeoTransform）：
 *   geoX = gt[0] + col * gt[1] + row * gt[2]
 *   geoY = gt[3] + col * gt[4] + row * gt[5]
 */
namespace GeoTransformUtils
{

/**
 * @brief 像素坐标 → 地理坐标
 * @param geoTransform  GDAL 六参数地理变换数组
 * @param col           像素列号
 * @param row           像素行号
 * @return              地理坐标 (X, Y)
 */
QPair<double, double> pixelToGeo(const QVector<double>& geoTransform,
                                  double col, double row);

/**
 * @brief 地理坐标 → 像素坐标
 * @param geoTransform  GDAL 六参数地理变换数组
 * @param geoX          地理 X 坐标
 * @param geoY          地理 Y 坐标
 * @return              像素坐标 (col, row)
 */
QPair<double, double> geoToPixel(const QVector<double>& geoTransform,
                                  double geoX, double geoY);

/**
 * @brief 计算两幅影像在地理空间中的重叠区域
 *
 * 分别计算两幅影像四角地理范围，求交集后转换回各自的像素坐标系。
 * 返回的重叠区域可用于分块处理时确定读写窗口位置。
 *
 * @param gt1     影像1的地理变换
 * @param w1, h1  影像1的宽、高（像素）
 * @param gt2     影像2的地理变换
 * @param w2, h2  影像2的宽、高（像素）
 * @param[out] xOff, yOff, xSize, ySize    影像1中的重叠窗口
 * @param[out] xOff2, yOff2, xSize2, ySize2 影像2中的重叠窗口
 * @return        存在有效重叠区域时返回 true
 */
bool computeOverlap(const QVector<double>& gt1, int w1, int h1,
                    const QVector<double>& gt2, int w2, int h2,
                    int& xOff, int& yOff, int& xSize, int& ySize,
                    int& xOff2, int& yOff2, int& xSize2, int& ySize2);

} // namespace GeoTransformUtils

#endif // GEOTRANSFORMUTILS_H
