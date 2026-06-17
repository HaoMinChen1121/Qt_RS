#include "GcpMatcher.h"

#include <gdal_priv.h>
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <QDebug>
#include <cmath>

// SURF in opencv_contrib, if unavailable fall back to SIFT
#ifdef HAVE_OPENCV_XFEATURES2D
#include <opencv2/xfeatures2d.hpp>
#endif

// Target max dimension for overview reading — large enough for SIFT/SURF to find
// distinctive features, small enough to stay under ~200 MB per image.
static const int OVERVIEW_TARGET_DIM = 4000;

// ── Determine best overview level and output dimensions ──
// Prefers explicit GDAL overview pyramids (built with averaging, high quality).
// Falls back to RasterIO decimation if no overviews exist.
static void computeOverviewScale(GDALDataset* ds, int bandIdx,
                                  int& ovW, int& ovH, double& sx, double& sy,
                                  int& outOvLevel)
{
    int fullW = ds->GetRasterXSize();
    int fullH = ds->GetRasterYSize();
    int maxDim = std::max(fullW, fullH);

    if (maxDim <= OVERVIEW_TARGET_DIM)
    {
        ovW = fullW; ovH = fullH;
        sx = 1.0; sy = 1.0;
        outOvLevel = -1;
        return;
    }

    GDALRasterBand* band = ds->GetRasterBand(bandIdx);
    int nOvs = band ? band->GetOverviewCount() : 0;

    // Search for the best explicit overview level (closest to target, not smaller)
    int bestLev = -1;
    int bestDim = 0;
    for (int i = 0; i < nOvs; ++i)
    {
        GDALRasterBand* ovBand = band->GetOverview(i);
        if (!ovBand) continue;
        int od = std::max(ovBand->GetXSize(), ovBand->GetYSize());
        // Pick the smallest overview that exceeds half the target
        if (od >= OVERVIEW_TARGET_DIM / 2 && (bestLev < 0 || od < bestDim))
        {
            bestLev = i;
            bestDim = od;
        }
    }

    if (bestLev >= 0)
    {
        GDALRasterBand* ovBand = band->GetOverview(bestLev);
        ovW = ovBand->GetXSize();
        ovH = ovBand->GetYSize();
        sx = (double)fullW / ovW;
        sy = (double)fullH / ovH;
        outOvLevel = bestLev;
    }
    else
    {
        // No suitable overview — RasterIO decimation fallback, keep higher resolution
        double ratio = (double)maxDim / OVERVIEW_TARGET_DIM;
        ovW = (int)(fullW / ratio + 0.5);
        ovH = (int)(fullH / ratio + 0.5);
        if (ovW < 1) ovW = 1;
        if (ovH < 1) ovH = 1;
        sx = (double)fullW / ovW;
        sy = (double)fullH / ovH;
        outOvLevel = -1;
    }
}

// ── Read a band into a float buffer, using explicit overviews when available ──
static bool readBandOverview(GDALDataset* ds, int bandIdx,
                              float* buf, int ovW, int ovH,
                              int ovLevel,
                              double& outMin, double& outMax)
{
    GDALRasterBand* band = ds->GetRasterBand(bandIdx);
    if (!band) return false;

    CPLErr e;
    if (ovLevel >= 0)
    {
        // Use explicit overview band (high-quality averaging pyramid)
        GDALRasterBand* ovBand = band->GetOverview(ovLevel);
        if (!ovBand) return false;
        e = ovBand->RasterIO(GF_Read, 0, 0, ovW, ovH,
                              buf, ovW, ovH, GDT_Float32, 0, 0);
    }
    else
    {
        // Fallback: let GDAL decimate (uses nearest-neighbour, but better than nothing)
        int fullW = ds->GetRasterXSize();
        int fullH = ds->GetRasterYSize();
        e = band->RasterIO(GF_Read, 0, 0, fullW, fullH,
                            buf, ovW, ovH, GDT_Float32, 0, 0);
    }
    if (e != CE_None) return false;

    outMin = 1e30; outMax = -1e30;
    for (int i = 0; i < ovW * ovH; ++i)
    {
        float v = buf[i];
        if (v < outMin) outMin = v;
        if (v > outMax) outMax = v;
    }
    return true;
}

// ── GDAL → cv::Mat (overview-based, low memory) ──
// scaleX/scaleY: multiply overview pixel coords to get full-resolution coords
static cv::Mat gdalToCvMat(const QString& path, bool toGray,
                            int& outW, int& outH,
                            double& scaleX, double& scaleY)
{
    GDALAllRegister();
    GDALDataset* ds = (GDALDataset*)GDALOpen(path.toUtf8().constData(), GA_ReadOnly);
    if (!ds)
    {
        qWarning() << "[GcpMatcher] Cannot open:" << path;
        return cv::Mat();
    }

    int ovW, ovH, ovLevel;
    computeOverviewScale(ds, 1, ovW, ovH, scaleX, scaleY, ovLevel);

    outW = ovW;
    outH = ovH;
    int bands = ds->GetRasterCount();

    if (toGray || bands == 1)
    {
        // Single band → grayscale
        QVector<float> buf(ovW * ovH);
        double vMin, vMax;
        if (!readBandOverview(ds, 1, buf.data(), ovW, ovH, ovLevel, vMin, vMax))
        {
            GDALClose(ds);
            return cv::Mat();
        }
        double range = vMax - vMin;
        if (range < 1e-10) range = 1;

        cv::Mat gray(ovH, ovW, CV_8UC1);
        for (int r = 0; r < ovH; ++r)
        {
            uchar* row = gray.ptr<uchar>(r);
            for (int c = 0; c < ovW; ++c)
            {
                float v = buf[r * ovW + c];
                row[c] = (uchar)((v - vMin) / range * 255.0);
            }
        }
        GDALClose(ds);
        return gray;
    }
    else
    {
        // 3 bands → BGR
        int readBands = std::min(3, bands);
        cv::Mat color(ovH, ovW, CV_8UC3);

        for (int b = 0; b < readBands; ++b)
        {
            int srcBand = b + 1;
            if (srcBand > bands) srcBand = bands;
            QVector<float> buf(ovW * ovH);
            double vMin, vMax;
            if (!readBandOverview(ds, srcBand, buf.data(), ovW, ovH, ovLevel, vMin, vMax))
            {
                GDALClose(ds);
                return cv::Mat();
            }
            double range = vMax - vMin;
            if (range < 1e-10) range = 1;

            // OpenCV BGR: b=0→R→ch2, b=1→G→ch1, b=2→B→ch0
            int ch = 2 - b;
            for (int r = 0; r < ovH; ++r)
            {
                uchar* row = color.ptr<uchar>(r);
                for (int c = 0; c < ovW; ++c)
                {
                    float v = buf[r * ovW + c];
                    row[c * 3 + ch] = (uchar)((v - vMin) / range * 255.0);
                }
            }
        }
        GDALClose(ds);
        return color;
    }
}

// ── Read a full-resolution window from a GDAL dataset ──
static cv::Mat gdalReadWindow(const QString& path, bool toGray,
                               int xOff, int yOff, int w, int h)
{
    GDALAllRegister();
    GDALDataset* ds = (GDALDataset*)GDALOpen(path.toUtf8().constData(), GA_ReadOnly);
    if (!ds) return cv::Mat();

    int bands = ds->GetRasterCount();

    if (toGray || bands == 1)
    {
        GDALRasterBand* b = ds->GetRasterBand(1);
        if (!b) { GDALClose(ds); return cv::Mat(); }
        QVector<float> buf(w * h);
        CPLErr e = b->RasterIO(GF_Read, xOff, yOff, w, h,
            buf.data(), w, h, GDT_Float32, 0, 0);
        if (e != CE_None) { GDALClose(ds); return cv::Mat(); }

        double vMin = 1e30, vMax = -1e30;
        for (int i = 0; i < w * h; ++i)
        {
            float v = buf[i];
            if (v < vMin) vMin = v;
            if (v > vMax) vMax = v;
        }
        double range = vMax - vMin;
        if (range < 1e-10) range = 1;

        cv::Mat gray(h, w, CV_8UC1);
        for (int r = 0; r < h; ++r)
        {
            uchar* row = gray.ptr<uchar>(r);
            for (int c = 0; c < w; ++c)
            {
                float v = buf[r * w + c];
                row[c] = (uchar)((v - vMin) / range * 255.0);
            }
        }
        GDALClose(ds);
        return gray;
    }
    else
    {
        int readBands = std::min(3, bands);
        cv::Mat color(h, w, CV_8UC3);
        for (int b = 0; b < readBands; ++b)
        {
            int srcBand = b + 1;
            if (srcBand > bands) srcBand = bands;
            GDALRasterBand* rBand = ds->GetRasterBand(srcBand);
            if (!rBand) { GDALClose(ds); return cv::Mat(); }
            QVector<float> buf(w * h);
            CPLErr e = rBand->RasterIO(GF_Read, xOff, yOff, w, h,
                buf.data(), w, h, GDT_Float32, 0, 0);
            if (e != CE_None) { GDALClose(ds); return cv::Mat(); }

            double vMin = 1e30, vMax = -1e30;
            for (int i = 0; i < w * h; ++i)
            {
                float v = buf[i];
                if (v < vMin) vMin = v;
                if (v > vMax) vMax = v;
            }
            double range = vMax - vMin;
            if (range < 1e-10) range = 1;

            int ch = 2 - b;
            for (int r = 0; r < h; ++r)
            {
                uchar* row = color.ptr<uchar>(r);
                for (int c = 0; c < w; ++c)
                {
                    float v = buf[r * w + c];
                    row[c * 3 + ch] = (uchar)((v - vMin) / range * 255.0);
                }
            }
        }
        GDALClose(ds);
        return color;
    }
}

// ── RANSAC 过滤 ──
static QVector<Gcp> ransacFilter(const QVector<Gcp>& rawGcps,
                                 const GcpMatchingParams& params,
                                 int* outInliers)
{
    if (rawGcps.size() < 4)
    {
        if (outInliers) *outInliers = rawGcps.size();
        return rawGcps;
    }

    std::vector<cv::Point2f> srcPts, refPts;
    srcPts.reserve(rawGcps.size());
    refPts.reserve(rawGcps.size());
    for (const auto& g : rawGcps)
    {
        srcPts.push_back(cv::Point2f((float)g.srcX, (float)g.srcY));
        refPts.push_back(cv::Point2f((float)g.refX, (float)g.refY));
    }

    std::vector<uchar> mask;
    cv::findHomography(srcPts, refPts, cv::RANSAC, params.ransacThreshold, mask);

    QVector<Gcp> inliers;
    for (size_t i = 0; i < mask.size(); ++i)
    {
        if (mask[i])
        {
            Gcp g = rawGcps[(int)i];
            g.isAuto = true;
            inliers.append(g);
        }
    }

    if (outInliers) *outInliers = inliers.size();
    return inliers;
}

// ═══════════════════════════════════════════════════
//  SIFT 匹配 (overview-based)
// ═══════════════════════════════════════════════════

QVector<Gcp> GcpMatcher::matchSIFT(const QString& srcImage,
                                   const QString& refImage,
                                   const GcpMatchingParams& params)
{
    int srcW, srcH, refW, refH;
    double srcSx, srcSy, refSx, refSy;
    cv::Mat srcMat = gdalToCvMat(srcImage, false, srcW, srcH, srcSx, srcSy);
    cv::Mat refMat = gdalToCvMat(refImage, false, refW, refH, refSx, refSy);

    if (srcMat.empty() || refMat.empty())
    {
        qWarning() << "[GcpMatcher] Empty image for SIFT matching";
        return {};
    }

    cv::Mat srcGray, refGray;
    cv::cvtColor(srcMat, srcGray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(refMat, refGray, cv::COLOR_BGR2GRAY);

    auto sift = cv::SIFT::create(params.maxFeatures);
    std::vector<cv::KeyPoint> kp1, kp2;
    cv::Mat desc1, desc2;
    sift->detectAndCompute(srcGray, cv::noArray(), kp1, desc1);
    sift->detectAndCompute(refGray, cv::noArray(), kp2, desc2);

    if (kp1.empty() || kp2.empty() || desc1.empty() || desc2.empty())
    {
        qWarning() << "[GcpMatcher] No SIFT keypoints found";
        return {};
    }

    cv::FlannBasedMatcher matcher;
    std::vector<std::vector<cv::DMatch>> knnMatches;
    matcher.knnMatch(desc1, desc2, knnMatches, 2);

    // Lowe's ratio test; scale coords back to full resolution
    QVector<Gcp> rawGcps;
    for (const auto& m : knnMatches)
    {
        if (m.size() < 2) continue;
        if (m[0].distance < params.ratioThreshold * m[1].distance)
        {
            Gcp g;
            g.srcX = kp1[m[0].queryIdx].pt.x * srcSx;
            g.srcY = kp1[m[0].queryIdx].pt.y * srcSy;
            g.refX = kp2[m[0].trainIdx].pt.x * refSx;
            g.refY = kp2[m[0].trainIdx].pt.y * refSy;
            g.isAuto = true;
            rawGcps.append(g);
        }
    }

    qDebug() << "[GcpMatcher] SIFT raw matches:" << rawGcps.size();
    return rawGcps;
}

// ═══════════════════════════════════════════════════
//  SURF 匹配 (overview-based)
// ═══════════════════════════════════════════════════

QVector<Gcp> GcpMatcher::matchSURF(const QString& srcImage,
                                   const QString& refImage,
                                   const GcpMatchingParams& params)
{
#ifdef HAVE_OPENCV_XFEATURES2D
    int srcW, srcH, refW, refH;
    double srcSx, srcSy, refSx, refSy;
    cv::Mat srcMat = gdalToCvMat(srcImage, false, srcW, srcH, srcSx, srcSy);
    cv::Mat refMat = gdalToCvMat(refImage, false, refW, refH, refSx, refSy);

    if (srcMat.empty() || refMat.empty())
        return {};

    cv::Mat srcGray, refGray;
    cv::cvtColor(srcMat, srcGray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(refMat, refGray, cv::COLOR_BGR2GRAY);

    auto surf = cv::xfeatures2d::SURF::create(400, 4, 3, false, false);
    std::vector<cv::KeyPoint> kp1, kp2;
    cv::Mat desc1, desc2;
    surf->detectAndCompute(srcGray, cv::noArray(), kp1, desc1);
    surf->detectAndCompute(refGray, cv::noArray(), kp2, desc2);

    if (kp1.empty() || kp2.empty())
        return {};

    cv::FlannBasedMatcher matcher;
    std::vector<std::vector<cv::DMatch>> knnMatches;
    matcher.knnMatch(desc1, desc2, knnMatches, 2);

    QVector<Gcp> rawGcps;
    for (const auto& m : knnMatches)
    {
        if (m.size() < 2) continue;
        if (m[0].distance < params.ratioThreshold * m[1].distance)
        {
            Gcp g;
            g.srcX = kp1[m[0].queryIdx].pt.x * srcSx;
            g.srcY = kp1[m[0].queryIdx].pt.y * srcSy;
            g.refX = kp2[m[0].trainIdx].pt.x * refSx;
            g.refY = kp2[m[0].trainIdx].pt.y * refSy;
            g.isAuto = true;
            rawGcps.append(g);
        }
    }

    qDebug() << "[GcpMatcher] SURF raw matches:" << rawGcps.size();
    return rawGcps;
#else
    qWarning() << "[GcpMatcher] SURF not available, falling back to SIFT";
    return matchSIFT(srcImage, refImage, params);
#endif
}

// ═══════════════════════════════════════════════════
//  NCC 自动模板匹配 (overview-based)
// ═══════════════════════════════════════════════════

QVector<Gcp> GcpMatcher::matchNCC(const QString& srcImage,
                                  const QString& refImage,
                                  const GcpMatchingParams& params)
{
    int srcW, srcH, refW, refH;
    double srcSx, srcSy, refSx, refSy;
    cv::Mat srcMat = gdalToCvMat(srcImage, true, srcW, srcH, srcSx, srcSy);
    cv::Mat refMat = gdalToCvMat(refImage, true, refW, refH, refSx, refSy);

    if (srcMat.empty() || refMat.empty())
        return {};

    // Scale template parameters to overview resolution
    int tplSize = std::max(4, (int)(params.nccTemplateSize / std::max(srcSx, srcSy)));
    int searchWin = std::max(tplSize + 4, (int)(params.nccSearchWindow / std::max(refSx, refSy)));
    int step = tplSize;

    QVector<Gcp> results;
    for (int sy = tplSize / 2; sy + tplSize / 2 < srcH; sy += step)
    {
        for (int sx = tplSize / 2; sx + tplSize / 2 < srcW; sx += step)
        {
            int tx = sx - tplSize / 2;
            int ty = sy - tplSize / 2;
            if (tx < 0 || ty < 0 || tx + tplSize > srcW || ty + tplSize > srcH)
                continue;

            cv::Rect roi(tx, ty, tplSize, tplSize);
            cv::Mat tpl = srcMat(roi);

            int rx0 = std::max(0, sx - searchWin / 2);
            int ry0 = std::max(0, sy - searchWin / 2);
            int rw = std::min(refW - rx0, searchWin);
            int rh = std::min(refH - ry0, searchWin);
            if (rw < tplSize || rh < tplSize) continue;

            cv::Rect searchRoi(rx0, ry0, rw, rh);
            cv::Mat searchRegion = refMat(searchRoi);
            cv::Mat nccResult;
            cv::matchTemplate(searchRegion, tpl, nccResult, cv::TM_CCOEFF_NORMED);

            double minVal, maxVal;
            cv::Point minLoc, maxLoc;
            cv::minMaxLoc(nccResult, &minVal, &maxVal, &minLoc, &maxLoc);

            if (maxVal > 0.6)
            {
                Gcp g;
                g.srcX = sx * srcSx;
                g.srcY = sy * srcSy;
                g.refX = (rx0 + maxLoc.x + tplSize / 2.0) * refSx;
                g.refY = (ry0 + maxLoc.y + tplSize / 2.0) * refSy;
                g.isAuto = true;
                results.append(g);
            }
        }
    }

    qDebug() << "[GcpMatcher] NCC auto matches:" << results.size();
    return results;
}

// ═══════════════════════════════════════════════════
//  半自动精化 (windowed full-resolution reads)
// ═══════════════════════════════════════════════════

QVector<Gcp> GcpMatcher::refineByNCC(const QString& srcImage,
                                     const QString& refImage,
                                     const QVector<Gcp>& roughGcps,
                                     const GcpMatchingParams& params)
{
    if (roughGcps.isEmpty()) return {};

    int tplSize = params.nccTemplateSize;
    int searchWin = params.nccSearchWindow;
    int halfWin = searchWin / 2;

    QVector<Gcp> refined = roughGcps;
    for (auto& gcp : refined)
    {
        int sx = (int)gcp.srcX;
        int sy = (int)gcp.srcY;
        int rx = (int)gcp.refX;
        int ry = (int)gcp.refY;

        // Read template window from source at full resolution
        int tx = std::max(0, sx - tplSize / 2);
        int ty = std::max(0, sy - tplSize / 2);
        cv::Mat tpl = gdalReadWindow(srcImage, true, tx, ty, tplSize, tplSize);
        if (tpl.empty()) continue;

        // Read search window from reference at full resolution
        int rx0 = std::max(0, rx - halfWin);
        int ry0 = std::max(0, ry - halfWin);
        cv::Mat searchRegion = gdalReadWindow(refImage, true, rx0, ry0, searchWin, searchWin);
        if (searchRegion.empty()) continue;

        // Validate: search region must be large enough for the template
        if (searchRegion.cols < tpl.cols || searchRegion.rows < tpl.rows) continue;

        cv::Mat nccResult;
        cv::matchTemplate(searchRegion, tpl, nccResult, cv::TM_CCOEFF_NORMED);

        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(nccResult, &minVal, &maxVal, &minLoc, &maxLoc);

        // Sub-pixel refinement via quadratic interpolation
        if (maxVal > 0.5)
        {
            double subX = maxLoc.x, subY = maxLoc.y;
            if (maxLoc.x > 0 && maxLoc.x < nccResult.cols - 1)
            {
                double v0 = nccResult.at<float>(maxLoc.y, maxLoc.x - 1);
                double v1 = maxVal;
                double v2 = nccResult.at<float>(maxLoc.y, maxLoc.x + 1);
                double denom = 2.0 * (2.0 * v1 - v0 - v2);
                if (std::abs(denom) > 1e-10)
                    subX += (v0 - v2) / denom;
            }
            if (maxLoc.y > 0 && maxLoc.y < nccResult.rows - 1)
            {
                double v0 = nccResult.at<float>(maxLoc.y - 1, maxLoc.x);
                double v1 = maxVal;
                double v2 = nccResult.at<float>(maxLoc.y + 1, maxLoc.x);
                double denom = 2.0 * (2.0 * v1 - v0 - v2);
                if (std::abs(denom) > 1e-10)
                    subY += (v0 - v2) / denom;
            }
            gcp.refX = rx0 + subX + tpl.cols / 2.0;
            gcp.refY = ry0 + subY + tpl.rows / 2.0;
        }
    }

    return refined;
}

// ═══════════════════════════════════════════════════
//  自动匹配入口
// ═══════════════════════════════════════════════════

QVector<Gcp> GcpMatcher::autoMatch(const QString& srcImage,
                                   const QString& refImage,
                                   const GcpMatchingParams& params,
                                   int* outTotalMatches,
                                   int* outInliers)
{
    QVector<Gcp> rawGcps;

    if (params.method == "SURF")
        rawGcps = matchSURF(srcImage, refImage, params);
    else if (params.method == "NCC")
        rawGcps = matchNCC(srcImage, refImage, params);
    else // SIFT or default
        rawGcps = matchSIFT(srcImage, refImage, params);

    if (outTotalMatches) *outTotalMatches = rawGcps.size();

    if (rawGcps.isEmpty())
        return {};

    int inliers = 0;
    QVector<Gcp> filtered = ransacFilter(rawGcps, params, &inliers);

    if (outInliers) *outInliers = inliers;

    qDebug() << "[GcpMatcher] autoMatch:" << rawGcps.size()
             << "raw ->" << inliers << "inliers";

    return filtered;
}
