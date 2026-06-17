#include "GeoTransformUtils.h"
#include <algorithm>
#include <cmath>

namespace GeoTransformUtils
{

QPair<double, double> pixelToGeo(const QVector<double>& gt, double col, double row)
{
    // 地理变换至少需要6个参数
    if (gt.size() < 6)
        return {col, row};
    double x = gt[0] + col * gt[1] + row * gt[2];
    double y = gt[3] + col * gt[4] + row * gt[5];
    return {x, y};
}

QPair<double, double> geoToPixel(const QVector<double>& gt, double geoX, double geoY)
{
    if (gt.size() < 6)
        return {geoX, geoY};
    // 求解二维线性方程组：geoX = gt[0] + col*gt[1] + row*gt[2]
    //                      geoY = gt[3] + col*gt[4] + row*gt[5]
    double det = gt[1] * gt[5] - gt[2] * gt[4];
    if (std::abs(det) < 1e-12)
        return {0, 0};
    double col = (gt[5] * (geoX - gt[0]) - gt[2] * (geoY - gt[3])) / det;
    double row = (gt[1] * (geoY - gt[3]) - gt[4] * (geoX - gt[0])) / det;
    return {col, row};
}

bool computeOverlap(const QVector<double>& gt1, int w1, int h1,
                    const QVector<double>& gt2, int w2, int h2,
                    int& xOff, int& yOff, int& xSize, int& ySize,
                    int& xOff2, int& yOff2, int& xSize2, int& ySize2)
                    {
    // 影像1 的四角地理范围
    auto tl1 = pixelToGeo(gt1, 0, 0);
    auto br1 = pixelToGeo(gt1, w1, h1);
    double xMin1 = std::min(tl1.first, br1.first);
    double xMax1 = std::max(tl1.first, br1.first);
    double yMin1 = std::min(tl1.second, br1.second);
    double yMax1 = std::max(tl1.second, br1.second);

    // 影像2 的四角地理范围
    auto tl2 = pixelToGeo(gt2, 0, 0);
    auto br2 = pixelToGeo(gt2, w2, h2);
    double xMin2 = std::min(tl2.first, br2.first);
    double xMax2 = std::max(tl2.first, br2.first);
    double yMin2 = std::min(tl2.second, br2.second);
    double yMax2 = std::max(tl2.second, br2.second);

    // 交集的地理范围
    double oxMin = std::max(xMin1, xMin2);
    double oxMax = std::min(xMax1, xMax2);
    double oyMin = std::max(yMin1, yMin2);
    double oyMax = std::min(yMax1, yMax2);

    if (oxMin >= oxMax || oyMin >= oyMax)
        return false;

    // 将交集范围转换回各自的像素坐标系（注意 Y 轴方向）
    auto p1Min = geoToPixel(gt1, oxMin, oyMax);
    auto p1Max = geoToPixel(gt1, oxMax, oyMin);
    auto p2Min = geoToPixel(gt2, oxMin, oyMax);
    auto p2Max = geoToPixel(gt2, oxMax, oyMin);

    xOff  = std::max(0, (int)std::floor(std::min(p1Min.first, p1Max.first)));
    yOff  = std::max(0, (int)std::floor(std::min(p1Min.second, p1Max.second)));
    xSize = std::min(w1, (int)std::ceil(std::max(p1Min.first, p1Max.first))) - xOff;
    ySize = std::min(h1, (int)std::ceil(std::max(p1Min.second, p1Max.second))) - yOff;

    xOff2  = std::max(0, (int)std::floor(std::min(p2Min.first, p2Max.first)));
    yOff2  = std::max(0, (int)std::floor(std::min(p2Min.second, p2Max.second)));
    xSize2 = std::min(w2, (int)std::ceil(std::max(p2Min.first, p2Max.first))) - xOff2;
    ySize2 = std::min(h2, (int)std::ceil(std::max(p2Min.second, p2Max.second))) - yOff2;

    return xSize > 0 && ySize > 0 && xSize2 > 0 && ySize2 > 0;
}

} // namespace GeoTransformUtils
