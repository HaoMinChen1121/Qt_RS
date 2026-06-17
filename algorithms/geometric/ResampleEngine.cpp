#include "ResampleEngine.h"
#include "GcpModelSolver.h"
#include "cuda/ResampleKernels.h"
#include <cmath>
#include <algorithm>
#include <QDebug>

// ═══════════════════════════════════════════════════
//  GPU 可用性
// ═══════════════════════════════════════════════════

bool ResampleEngine::gpuAvailable()
{
    return CudaResample::isAvailable();
}

// ═══════════════════════════════════════════════════
//  CPU 回退实现
// ═══════════════════════════════════════════════════

bool ResampleEngine::processTileCPU(
    const float* srcData, int srcRegionW, int srcRegionH, int bands,
    int srcRegionX, int srcRegionY,
    const double* outGeoTrans,
    const GcpModel& correctionModel,
    const QVector<Gcp>& refGcps,
    int resampleIdx, float nodata,
    float* dstData, int dstX, int dstY, int dstW, int dstH)
{
    if (correctionModel.coefficients.isEmpty())
        return false;

    bool isTPS = correctionModel.type == "TPS";
    int polyOrder = 0;
    if (!isTPS)
        polyOrder = correctionModel.type.mid(10).toInt();

    // For TPS correction model, evalTPS needs kernel centers at reference coords.
    // The correction model was fitted with swapped GCPs (src↔ref), so evalTPS
    // expects .srcX/.srcY to be the reference coordinates.
    QVector<Gcp> tpsGcps;
    if (isTPS)
    {
        tpsGcps = refGcps;
        for (auto& g : tpsGcps)
        {
            std::swap(g.srcX, g.refX);
            std::swap(g.srcY, g.refY);
        }
    }

    for (int row = 0; row < dstH; ++row)
    {
        for (int col = 0; col < dstW; ++col)
        {
            // 输出像素地理坐标
            double geoX = outGeoTrans[0] + col * outGeoTrans[1] + row * outGeoTrans[2];
            double geoY = outGeoTrans[3] + col * outGeoTrans[4] + row * outGeoTrans[5];

            // 校正模型：ref → global src
            double srcX, srcY;
            if (isTPS)
                GcpModelSolver::evalModel(correctionModel, tpsGcps, geoX, geoY, srcX, srcY);
            else
                GcpModelSolver::evalPolynomial(correctionModel, polyOrder, geoX, geoY, srcX, srcY);

            // Convert to local buffer coordinates
            double localX = srcX - srcRegionX;
            double localY = srcY - srcRegionY;

            int dstIdx = (row * dstW + col) * bands;

            for (int b = 0; b < bands; ++b)
            {
                float v = nodata;

                if (resampleIdx == 0) // Nearest
                {
                    int ix = (int)(localX + 0.5);
                    int iy = (int)(localY + 0.5);
                    if (ix >= 0 && ix < srcRegionW && iy >= 0 && iy < srcRegionH)
                        v = srcData[(iy * srcRegionW + ix) * bands + b];
                }
                else if (resampleIdx == 1) // Bilinear
                {
                    if (localX >= -0.5 && localY >= -0.5 && localX < srcRegionW + 0.5 && localY < srcRegionH + 0.5)
                    {
                        double cx = std::max(0.0, std::min((double)srcRegionW - 1.001, localX));
                        double cy = std::max(0.0, std::min((double)srcRegionH - 1.001, localY));
                        int ix0 = (int)std::floor(cx);
                        int iy0 = (int)std::floor(cy);
                        int ix1 = std::min(ix0 + 1, srcRegionW - 1);
                        int iy1 = std::min(iy0 + 1, srcRegionH - 1);
                        if (ix0 < 0) ix0 = 0;
                        if (iy0 < 0) iy0 = 0;
                        double fx = cx - ix0;
                        double fy = cy - iy0;

                        float v00 = srcData[(iy0 * srcRegionW + ix0) * bands + b];
                        float v10 = srcData[(iy0 * srcRegionW + ix1) * bands + b];
                        float v01 = srcData[(iy1 * srcRegionW + ix0) * bands + b];
                        float v11 = srcData[(iy1 * srcRegionW + ix1) * bands + b];

                        double v0 = v00 + (v10 - v00) * fx;
                        double v1 = v01 + (v11 - v01) * fx;
                        v = (float)(v0 + (v1 - v0) * fy);
                    }
                }
                else if (resampleIdx == 2) // Cubic
                {
                    if (localX >= 0 && localY >= 0 && localX <= srcRegionW - 1 && localY <= srcRegionH - 1)
                    {
                        int ix = (int)std::floor(localX);
                        int iy = (int)std::floor(localY);
                        double fx = localX - ix;
                        double fy = localY - iy;

                        auto cubicW = [](double t) -> double {
                            double a = -0.5;
                            double at = std::abs(t);
                            if (at < 1.0)
                                return (a + 2.0) * at * at * at - (a + 3.0) * at * at + 1.0;
                            else if (at < 2.0)
                                return a * at * at * at - 5.0 * a * at * at + 8.0 * a * at - 4.0 * a;
                            return 0.0;
                        };

                        double result = 0;
                        for (int j = 0; j < 4; ++j)
                        {
                            int ry = std::max(0, std::min(srcRegionH - 1, iy + j - 1));
                            double rw = cubicW((j - 1) - fy);
                            double colSum = 0;
                            for (int i = 0; i < 4; ++i)
                            {
                                int rx = std::max(0, std::min(srcRegionW - 1, ix + i - 1));
                                colSum += srcData[(ry * srcRegionW + rx) * bands + b] * cubicW((i - 1) - fx);
                            }
                            result += colSum * rw;
                        }
                        v = (float)result;
                    }
                }

                dstData[dstIdx + b] = v;
            }
        }
    }

    return true;
}

// ═══════════════════════════════════════════════════
//  调度入口
// ═══════════════════════════════════════════════════

bool ResampleEngine::processTile(
    const float* srcData, int srcRegionW, int srcRegionH, int bands,
    int srcRegionX, int srcRegionY,
    const double* outGeoTrans,
    const GcpModel& correctionModel,
    const QVector<Gcp>& refGcps,
    const QString& resampleMethod, float nodata,
    float* dstData, int dstX, int dstY, int dstW, int dstH)
{
    int resampleIdx = 0; // Nearest
    if (resampleMethod == "Bilinear") resampleIdx = 1;
    else if (resampleMethod == "Cubic") resampleIdx = 2;

    bool useGPU = gpuAvailable() && !correctionModel.coefficients.isEmpty();

    if (!useGPU)
        return processTileCPU(srcData, srcRegionW, srcRegionH, bands,
            srcRegionX, srcRegionY,
            outGeoTrans, correctionModel, refGcps,
            resampleIdx, nodata, dstData, dstX, dstY, dstW, dstH);

    // ── GPU 路径 ──
    bool isTPS = correctionModel.type == "TPS";
    int order = 0;
    if (correctionModel.type.startsWith("Polynomial"))
        order = correctionModel.type.mid(10).toInt();

    if (!isTPS)
    {
        // 多项式
        return CudaResample::resampleTilePoly(
            srcData, srcRegionW, srcRegionH, bands,
            srcRegionX, srcRegionY,
            correctionModel.coefficients.constData(), order,
            outGeoTrans, resampleIdx, nodata,
            dstData, dstX, dstY, dstW, dstH);
    }
    else
    {
        // TPS: 需要 ref GCP 坐标用于核函数评估
        int n = refGcps.size();
        QVector<double> gcpCoords(2 * n);
        for (int i = 0; i < n; ++i)
        {
            gcpCoords[2 * i]     = refGcps[i].refX;
            gcpCoords[2 * i + 1] = refGcps[i].refY;
        }

        return CudaResample::resampleTileTPS(
            srcData, srcRegionW, srcRegionH, bands,
            srcRegionX, srcRegionY,
            correctionModel.coefficients.constData(),
            gcpCoords.constData(), n,
            outGeoTrans, resampleIdx, nodata,
            dstData, dstX, dstY, dstW, dstH);
    }
}
