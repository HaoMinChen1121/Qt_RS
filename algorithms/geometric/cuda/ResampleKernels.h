#ifndef RESAMPLEKERNELS_H
#define RESAMPLEKERNELS_H

// CUDA-accelerated resampling engine
// HAS_CUDA macro enables GPU path; without it falls back to CPU

#ifdef __cplusplus
extern "C" {
#endif

// Check if CUDA device is available (runtime detection)
int cudaResampleAvailable(void);

// Polynomial model resampling for a single tile
// srcData is a sub-region of the full source image; srcRegionX/Y is its origin
int cudaResampleTilePoly(
    const float* srcData, int srcRegionW, int srcRegionH, int bands,
    int srcRegionX, int srcRegionY,
    const double* polyCoeffs, int polyOrder,
    const double* outGeoTrans, int resampleMethod, float nodata,
    float* dstData, int dstX, int dstY, int dstW, int dstH);

// TPS model resampling for a single tile
int cudaResampleTileTPS(
    const float* srcData, int srcRegionW, int srcRegionH, int bands,
    int srcRegionX, int srcRegionY,
    const double* tpsParams, const double* gcpSrcCoords, int numGcps,
    const double* outGeoTrans, int resampleMethod, float nodata,
    float* dstData, int dstX, int dstY, int dstW, int dstH);

// Device-pointer versions (src/dst already in GPU memory)
int cudaResampleTilePoly_device(
    const float* d_srcData, int srcRegionW, int srcRegionH, int bands,
    int srcRegionX, int srcRegionY,
    const double* polyCoeffs, int polyOrder,
    const double* outGeoTrans, int resampleMethod, float nodata,
    float* d_dstData, int dstX, int dstY, int dstW, int dstH);

int cudaResampleTileTPS_device(
    const float* d_srcData, int srcRegionW, int srcRegionH, int bands,
    int srcRegionX, int srcRegionY,
    const double* tpsParams, const double* gcpSrcCoords, int numGcps,
    const double* outGeoTrans, int resampleMethod, float nodata,
    float* d_dstData, int dstX, int dstY, int dstW, int dstH);

// GPU memory management wrappers (callable from code without cuda_runtime.h)
int cudaUploadSource(const float* srcData, size_t bytes, float** outDevicePtr);
int cudaFreeDevicePtr(float* d_ptr);
int cudaAllocDevice(size_t bytes, float** outPtr);
int cudaCopyFromDevice(float* hostDst, const float* d_src, size_t bytes);

#ifdef __cplusplus
}

namespace CudaResample
{
    inline bool isAvailable() { return cudaResampleAvailable() != 0; }

    inline bool resampleTilePoly(
        const float* srcData, int srcRegionW, int srcRegionH, int bands,
        int srcRegionX, int srcRegionY,
        const double* polyCoeffs, int polyOrder,
        const double* outGeoTrans, int resampleMethod, float nodata,
        float* dstData, int dstX, int dstY, int dstW, int dstH)
    {
        return cudaResampleTilePoly(srcData, srcRegionW, srcRegionH, bands,
            srcRegionX, srcRegionY,
            polyCoeffs, polyOrder, outGeoTrans, resampleMethod, nodata,
            dstData, dstX, dstY, dstW, dstH) == 0;
    }

    inline bool resampleTileTPS(
        const float* srcData, int srcRegionW, int srcRegionH, int bands,
        int srcRegionX, int srcRegionY,
        const double* tpsParams, const double* gcpSrcCoords, int numGcps,
        const double* outGeoTrans, int resampleMethod, float nodata,
        float* dstData, int dstX, int dstY, int dstW, int dstH)
    {
        return cudaResampleTileTPS(srcData, srcRegionW, srcRegionH, bands,
            srcRegionX, srcRegionY,
            tpsParams, gcpSrcCoords, numGcps, outGeoTrans, resampleMethod, nodata,
            dstData, dstX, dstY, dstW, dstH) == 0;
    }
}

#endif // __cplusplus

#endif // RESAMPLEKERNELS_H
