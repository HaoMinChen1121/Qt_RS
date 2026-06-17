#ifndef RESAMPLEENGINE_H
#define RESAMPLEENGINE_H

#include "domain/params/GeometricCorrectionParams.h"

// Resampling engine: prefers GPU, falls back to CPU when unavailable
class ResampleEngine
{
public:
    // Check if GPU is available
    static bool gpuAvailable();

    // Process a single tile (auto-selects GPU/CPU path)
    // srcData: sub-region of source image, BIP layout, float
    // srcRegionW/srcRegionH: dimensions of the sub-region buffer
    // srcRegionX/srcRegionY: origin of sub-region in full source image coordinates
    // outGeoTrans: 6-parameter output geotransform for this tile
    // correctionModel: ref->src correction model (pre-fitted by GeometricCorrector)
    // refGcps: original (unswapped) GCPs for TPS kernel evaluation
    static bool processTile(
        const float* srcData, int srcRegionW, int srcRegionH, int bands,
        int srcRegionX, int srcRegionY,
        const double* outGeoTrans,
        const GcpModel& correctionModel,
        const QVector<Gcp>& refGcps,
        const QString& resampleMethod, float nodata,
        float* dstData, int dstX, int dstY, int dstW, int dstH);

private:
    // CPU fallback implementation
    static bool processTileCPU(
        const float* srcData, int srcRegionW, int srcRegionH, int bands,
        int srcRegionX, int srcRegionY,
        const double* outGeoTrans,
        const GcpModel& correctionModel,
        const QVector<Gcp>& refGcps,
        int resampleIdx, float nodata,
        float* dstData, int dstX, int dstY, int dstW, int dstH);
};

#endif // RESAMPLEENGINE_H
