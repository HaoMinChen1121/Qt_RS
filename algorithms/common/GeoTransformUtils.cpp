#include "GeoTransformUtils.h"
#include <algorithm>
#include <cmath>
#include <ogr_srs_api.h>

namespace
{

void setLatLonMinMax(const double* lats, const double* lons, LatLonBounds& b)
{
    auto minmaxLat = std::minmax_element(lats, lats + 4);
    auto minmaxLon = std::minmax_element(lons, lons + 4);
    b.minLat = *minmaxLat.first;
    b.maxLat = *minmaxLat.second;
    b.minLon = *minmaxLon.first;
    b.maxLon = *minmaxLon.second;
    b.valid = true;
}

} // anonymous namespace

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

LatLonBounds computeLatLonBounds(const QVector<double>& gt,
                                  const QSize& rasterSize,
                                  const QString& projectionWkt)
{
    LatLonBounds bounds;
    if (gt.size() < 6 || rasterSize.isEmpty())
        return bounds;

    // 计算影像四角在原投影下的坐标
    QPair<double, double> corners[4] = {
        pixelToGeo(gt, 0, 0),
        pixelToGeo(gt, rasterSize.width(), 0),
        pixelToGeo(gt, rasterSize.width(), rasterSize.height()),
        pixelToGeo(gt, 0, rasterSize.height())
    };

    // 创建源 SRS
    OGRSpatialReferenceH hSrc = OSRNewSpatialReference(nullptr);
    QByteArray wkt = projectionWkt.toUtf8();
    char* pszWkt = wkt.data();
    if (OSRImportFromWkt(hSrc, &pszWkt) != OGRERR_NONE)
    {
        OSRDestroySpatialReference(hSrc);
        return bounds;
    }

    // 创建目标 SRS (WGS84)
    OGRSpatialReferenceH hDst = OSRNewSpatialReference(nullptr);
    OSRSetWellKnownGeogCS(hDst, "WGS84");

    // 若已是 WGS84 则直接读取四至
    if (OSRIsSame(hSrc, hDst))
    {
        double lats[4], lons[4];
        for (int i = 0; i < 4; ++i)
        {
            lons[i] = corners[i].first;
            lats[i] = corners[i].second;
        }
        setLatLonMinMax(lats, lons, bounds);
        OSRDestroySpatialReference(hSrc);
        OSRDestroySpatialReference(hDst);
        return bounds;
    }

    // 创建坐标转换
    OGRCoordinateTransformationH hCT = OCTNewCoordinateTransformation(hSrc, hDst);
    if (!hCT)
    {
        OSRDestroySpatialReference(hSrc);
        OSRDestroySpatialReference(hDst);
        return bounds;
    }

    double lats[4], lons[4];
    bool allOk = true;
    for (int i = 0; i < 4; ++i)
    {
        double x = corners[i].first;
        double y = corners[i].second;
        if (!OCTTransform(hCT, 1, &x, &y, nullptr))
        {
            allOk = false;
            break;
        }
        lons[i] = y;
        lats[i] = x;
    }

    OCTDestroyCoordinateTransformation(hCT);
    OSRDestroySpatialReference(hSrc);
    OSRDestroySpatialReference(hDst);

    if (!allOk)
        return bounds;

    setLatLonMinMax(lats, lons, bounds);

    return bounds;
}

QString formatDms(double dd, bool isLat)
{
    // 确定方向
    QString dir;
    if (isLat)
    {
        if (dd >= 0)
            dir = QStringLiteral("N");
        else
        {
            dir = QStringLiteral("S");
            dd = -dd;
        }
    }
    else
    {
        if (dd >= 0)
            dir = QStringLiteral("E");
        else
        {
            dir = QStringLiteral("W");
            dd = -dd;
        }
    }

    int d = static_cast<int>(dd);
    double mf = (dd - d) * 60.0;
    int m = static_cast<int>(mf);
    double s = (mf - m) * 60.0;

    // 处理秒的舍入问题（59.999... → 进位）
    if (s >= 59.9995)
    {
        s = 0.0;
        m++;
        if (m >= 60)
        {
            m = 0;
            d++;
        }
    }

    if (s < 0.05)
        return QStringLiteral("%1°%2′%3").arg(d).arg(m).arg(dir);
    else
        return QStringLiteral("%1°%2′%3″%4").arg(d).arg(m).arg(s, 0, 'f', 1).arg(dir);
}

} // namespace GeoTransformUtils
