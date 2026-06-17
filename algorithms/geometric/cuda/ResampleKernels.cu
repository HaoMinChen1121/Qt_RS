#ifdef __INTELLISENSE__
// IntelliSense doesn't understand CUDA syntax (<<<>>>, __device__, blockIdx, etc.)
// Suppress all errors by providing a minimal stub for IDE purposes only
#else
#endif

#include "ResampleKernels.h"
#include <cuda_runtime.h>
#include <cmath>

#ifndef __INTELLISENSE__

// NaN sentinel for out-of-bounds sampling
__device__ static float makeNaN() { return __int_as_float(0x7fffffff); }
__device__ static bool isNaN(float v) { return v != v; }

// ============================================================================
// Device: polynomial term helpers
// ============================================================================

__device__ static int termCount(int order)
{
    return (order + 1) * (order + 2) / 2;
}

// term k at (col, row), expanded as row^p * col^q, p+q <= order
__device__ static double polyTerm(int k, double col, double row, int order)
{
    int idx = 0;
    for (int d = 0; d <= order; ++d)
    {
        for (int p = 0; p <= d; ++p)
        {
            if (idx == k)
                return pow(row, p) * pow(col, d - p);
            ++idx;
        }
    }
    return 0.0;
}

// ============================================================================
// Device: bilinear interpolation (coordinates already in local buffer space)
// ============================================================================

__device__ static float bilinearSample(const float* srcData,
                                       int srcRegionW, int srcRegionH, int bands,
                                       double localX, double localY, int band)
{
    if (localX < -0.5 || localY < -0.5 || localX >= srcRegionW + 0.5 || localY >= srcRegionH + 0.5)
        return makeNaN();

    double cx = fmax(0.0, fmin(srcRegionW - 1.001, localX));
    double cy = fmax(0.0, fmin(srcRegionH - 1.001, localY));

    int ix0 = (int)floor(cx);
    int iy0 = (int)floor(cy);
    int ix1 = ix0 + 1;
    int iy1 = iy0 + 1;

    double fx = cx - ix0;
    double fy = cy - iy0;

    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 >= srcRegionW) ix1 = srcRegionW - 1;
    if (iy1 >= srcRegionH) iy1 = srcRegionH - 1;

    // BIP layout: bands interleaved per pixel
    float v00 = srcData[(iy0 * srcRegionW + ix0) * bands + band];
    float v10 = srcData[(iy0 * srcRegionW + ix1) * bands + band];
    float v01 = srcData[(iy1 * srcRegionW + ix0) * bands + band];
    float v11 = srcData[(iy1 * srcRegionW + ix1) * bands + band];

    double v0 = v00 + (v10 - v00) * fx;
    double v1 = v01 + (v11 - v01) * fx;

    return (float)(v0 + (v1 - v0) * fy);
}

// ============================================================================
// Device: cubic convolution interpolation (coordinates already in local buffer space)
// ============================================================================

__device__ static double cubicWeight(double t)
{
    double a = -0.5;
    double at = fabs(t);
    if (at < 1.0)
        return (a + 2.0) * at * at * at - (a + 3.0) * at * at + 1.0;
    else if (at < 2.0)
        return a * at * at * at - 5.0 * a * at * at + 8.0 * a * at - 4.0 * a;
    else
        return 0.0;
}

__device__ static float cubicSample(const float* srcData,
                                    int srcRegionW, int srcRegionH, int bands,
                                    double localX, double localY, int band)
{
    if (localX < 0.0 || localY < 0.0 || localX > srcRegionW - 1.0 || localY > srcRegionH - 1.0)
        return makeNaN();

    int ix = (int)floor(localX);
    int iy = (int)floor(localY);
    double fx = localX - ix;
    double fy = localY - iy;

    double result = 0.0;
    double rowWeights[4];
    for (int j = 0; j < 4; ++j)
        rowWeights[j] = cubicWeight((j - 1) - fy);

    for (int j = 0; j < 4; ++j)
    {
        int ry = iy + j - 1;
        if (ry < 0) ry = 0;
        if (ry >= srcRegionH) ry = srcRegionH - 1;

        double colSum = 0.0;
        for (int i = 0; i < 4; ++i)
        {
            int rx = ix + i - 1;
            if (rx < 0) rx = 0;
            if (rx >= srcRegionW) rx = srcRegionW - 1;

            float v = srcData[(ry * srcRegionW + rx) * bands + band];
            colSum += v * cubicWeight((i - 1) - fx);
        }
        result += colSum * rowWeights[j];
    }

    return (float)result;
}

// ============================================================================
// Kernel: polynomial correction (single tile)
// ============================================================================

__constant__ double c_polyCoeffs[128];

__global__ void corrPolyKernel(
    const float* __restrict__ srcData,
    int srcRegionW, int srcRegionH, int bands,
    int srcRegionX, int srcRegionY,
    int polyOrder,
    const double* __restrict__ outGeoTrans,
    int resampleMethod, float nodata,
    float* __restrict__ dstData,
    int dstTileW, int dstTileH)
{
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    int row = blockIdx.y * blockDim.y + threadIdx.y;

    if (col >= dstTileW || row >= dstTileH)
        return;

    int dstIdx = (row * dstTileW + col) * bands;

    // output pixel -> geographic coordinates
    double geoX = outGeoTrans[0] + col * outGeoTrans[1] + row * outGeoTrans[2];
    double geoY = outGeoTrans[3] + col * outGeoTrans[4] + row * outGeoTrans[5];

    // evaluate polynomial: (geoX, geoY) -> global (srcX, srcY)
    int nTerm = termCount(polyOrder);
    double srcX = 0, srcY = 0;
    for (int k = 0; k < nTerm; ++k)
    {
        double term = polyTerm(k, geoX, geoY, polyOrder);
        srcX += c_polyCoeffs[k] * term;
        srcY += c_polyCoeffs[nTerm + k] * term;
    }

    // Convert to local buffer coordinates
    double localX = srcX - srcRegionX;
    double localY = srcY - srcRegionY;

    // resample each band
    for (int b = 0; b < bands; ++b)
    {
        float v = nodata;

        if (resampleMethod == 0) // Nearest
        {
            int ix = (int)(localX + 0.5);
            int iy = (int)(localY + 0.5);
            if (ix >= 0 && ix < srcRegionW && iy >= 0 && iy < srcRegionH)
                v = srcData[(iy * srcRegionW + ix) * bands + b];
        }
        else if (resampleMethod == 1) // Bilinear
        {
            float bv = bilinearSample(srcData, srcRegionW, srcRegionH, bands, localX, localY, b);
            if (!isNaN(bv)) v = bv;
        }
        else if (resampleMethod == 2) // Cubic
        {
            float cv = cubicSample(srcData, srcRegionW, srcRegionH, bands, localX, localY, b);
            if (!isNaN(cv)) v = cv;
        }

        dstData[dstIdx + b] = v;
    }
}

// ============================================================================
// Kernel: TPS correction (single tile)
// ============================================================================

__global__ void corrTpsKernel(
    const float* __restrict__ srcData,
    int srcRegionW, int srcRegionH, int bands,
    int srcRegionX, int srcRegionY,
    const double* __restrict__ tpsParams,
    const double* __restrict__ gcpRefCoords,
    int numGcps,
    const double* __restrict__ outGeoTrans,
    int resampleMethod, float nodata,
    float* __restrict__ dstData,
    int dstTileW, int dstTileH)
{
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    int row = blockIdx.y * blockDim.y + threadIdx.y;

    if (col >= dstTileW || row >= dstTileH)
        return;

    int dstIdx = (row * dstTileW + col) * bands;
    int m = numGcps + 3;

    double geoX = outGeoTrans[0] + col * outGeoTrans[1] + row * outGeoTrans[2];
    double geoY = outGeoTrans[3] + col * outGeoTrans[4] + row * outGeoTrans[5];

    // TPS evaluation: (geoX, geoY) -> global (srcX, srcY)
    // affine part
    double srcX = tpsParams[numGcps + 0]
                + tpsParams[numGcps + 1] * geoX
                + tpsParams[numGcps + 2] * geoY;

    double srcY = tpsParams[m + numGcps + 0]
                + tpsParams[m + numGcps + 1] * geoX
                + tpsParams[m + numGcps + 2] * geoY;

    // warp part
    for (int i = 0; i < numGcps; ++i)
    {
        double dx = geoX - gcpRefCoords[2 * i];
        double dy = geoY - gcpRefCoords[2 * i + 1];
        double r2 = dx * dx + dy * dy;
        double phi = (r2 < 1e-30) ? 0.0 : r2 * log(sqrt(r2));

        srcX += tpsParams[i] * phi;
        srcY += tpsParams[m + i] * phi;
    }

    // Convert to local buffer coordinates
    double localX = srcX - srcRegionX;
    double localY = srcY - srcRegionY;

    for (int b = 0; b < bands; ++b)
    {
        float v = nodata;

        if (resampleMethod == 0)
        {
            int ix = (int)(localX + 0.5);
            int iy = (int)(localY + 0.5);
            if (ix >= 0 && ix < srcRegionW && iy >= 0 && iy < srcRegionH)
                v = srcData[(iy * srcRegionW + ix) * bands + b];
        }
        else if (resampleMethod == 1)
        {
            float bv = bilinearSample(srcData, srcRegionW, srcRegionH, bands, localX, localY, b);
            if (!isNaN(bv)) v = bv;
        }
        else if (resampleMethod == 2)
        {
            float cv = cubicSample(srcData, srcRegionW, srcRegionH, bands, localX, localY, b);
            if (!isNaN(cv)) v = cv;
        }

        dstData[dstIdx + b] = v;
    }
}

// ============================================================================
// Host launch functions
// ============================================================================

static const int BLOCK_DIM = 16;

extern "C" {

int cudaResampleAvailable(void)
{
    int count = 0;
    cudaError_t e = cudaGetDeviceCount(&count);
    return (e == cudaSuccess && count > 0) ? 1 : 0;
}

int cudaResampleTilePoly(
    const float* srcData, int srcRegionW, int srcRegionH, int bands,
    int srcRegionX, int srcRegionY,
    const double* polyCoeffs, int polyOrder,
    const double* outGeoTrans, int resampleMethod, float nodata,
    float* dstData, int dstX, int dstY, int dstW, int dstH)
{
    if (!cudaResampleAvailable()) return -1;

    int nTerm = (polyOrder + 1) * (polyOrder + 2) / 2;
    int coeffCount = 2 * nTerm;

    cudaError_t e = cudaMemcpyToSymbol(c_polyCoeffs, polyCoeffs,
        coeffCount * sizeof(double), 0, cudaMemcpyHostToDevice);
    if (e != cudaSuccess) return -1;

    float *d_src = nullptr, *d_dst = nullptr;
    double* d_geo = nullptr;

    size_t srcBytes = (size_t)srcRegionW * srcRegionH * bands * sizeof(float);
    size_t dstBytes = (size_t)dstW * dstH * bands * sizeof(float);

    dim3 block, grid;

    if (cudaMalloc(&d_src, srcBytes) != cudaSuccess) goto fail;
    if (cudaMalloc(&d_dst, dstBytes) != cudaSuccess) goto fail;
    if (cudaMalloc(&d_geo, 6 * sizeof(double)) != cudaSuccess) goto fail;

    if (cudaMemcpy(d_src, srcData, srcBytes, cudaMemcpyHostToDevice) != cudaSuccess) goto fail;
    if (cudaMemcpy(d_geo, outGeoTrans, 6 * sizeof(double), cudaMemcpyHostToDevice) != cudaSuccess) goto fail;

    block = dim3(BLOCK_DIM, BLOCK_DIM);
    grid  = dim3((dstW + BLOCK_DIM - 1) / BLOCK_DIM,
                 (dstH + BLOCK_DIM - 1) / BLOCK_DIM);

    corrPolyKernel<<<grid, block>>>(
        d_src, srcRegionW, srcRegionH, bands,
        srcRegionX, srcRegionY,
        polyOrder, d_geo,
        resampleMethod, nodata,
        d_dst, dstW, dstH);

    e = cudaGetLastError();
    if (e != cudaSuccess) goto fail;

    if (cudaMemcpy(dstData, d_dst, dstBytes, cudaMemcpyDeviceToHost) != cudaSuccess) goto fail;

    cudaFree(d_src);
    cudaFree(d_dst);
    cudaFree(d_geo);
    return 0;

fail:
    if (d_src) cudaFree(d_src);
    if (d_dst) cudaFree(d_dst);
    if (d_geo) cudaFree(d_geo);
    return -1;
}

int cudaResampleTileTPS(
    const float* srcData, int srcRegionW, int srcRegionH, int bands,
    int srcRegionX, int srcRegionY,
    const double* tpsParams, const double* gcpSrcCoords, int numGcps,
    const double* outGeoTrans, int resampleMethod, float nodata,
    float* dstData, int dstX, int dstY, int dstW, int dstH)
{
    if (!cudaResampleAvailable()) return -1;

    int m = numGcps + 3;
    int paramCount = 2 * m;

    float *d_src = nullptr, *d_dst = nullptr;
    double *d_tps = nullptr, *d_gcp = nullptr, *d_geo = nullptr;

    size_t srcBytes = (size_t)srcRegionW * srcRegionH * bands * sizeof(float);
    size_t dstBytes = (size_t)dstW * dstH * bands * sizeof(float);

    dim3 block, grid;

    if (cudaMalloc(&d_src, srcBytes) != cudaSuccess) goto fail;
    if (cudaMalloc(&d_dst, dstBytes) != cudaSuccess) goto fail;
    if (cudaMalloc(&d_tps, paramCount * sizeof(double)) != cudaSuccess) goto fail;
    if (cudaMalloc(&d_gcp, 2 * numGcps * sizeof(double)) != cudaSuccess) goto fail;
    if (cudaMalloc(&d_geo, 6 * sizeof(double)) != cudaSuccess) goto fail;

    if (cudaMemcpy(d_src, srcData, srcBytes, cudaMemcpyHostToDevice) != cudaSuccess) goto fail;
    if (cudaMemcpy(d_tps, tpsParams, paramCount * sizeof(double), cudaMemcpyHostToDevice) != cudaSuccess) goto fail;
    if (cudaMemcpy(d_gcp, gcpSrcCoords, 2 * numGcps * sizeof(double), cudaMemcpyHostToDevice) != cudaSuccess) goto fail;
    if (cudaMemcpy(d_geo, outGeoTrans, 6 * sizeof(double), cudaMemcpyHostToDevice) != cudaSuccess) goto fail;

    block = dim3(BLOCK_DIM, BLOCK_DIM);
    grid  = dim3((dstW + BLOCK_DIM - 1) / BLOCK_DIM,
                 (dstH + BLOCK_DIM - 1) / BLOCK_DIM);

    corrTpsKernel<<<grid, block>>>(
        d_src, srcRegionW, srcRegionH, bands,
        srcRegionX, srcRegionY,
        d_tps, d_gcp, numGcps, d_geo,
        resampleMethod, nodata,
        d_dst, dstW, dstH);

    if (cudaGetLastError() != cudaSuccess) goto fail;

    if (cudaMemcpy(dstData, d_dst, dstBytes, cudaMemcpyDeviceToHost) != cudaSuccess) goto fail;

    cudaFree(d_src);
    cudaFree(d_dst);
    cudaFree(d_tps);
    cudaFree(d_gcp);
    cudaFree(d_geo);
    return 0;

fail:
    if (d_src) cudaFree(d_src);
    if (d_dst) cudaFree(d_dst);
    if (d_tps) cudaFree(d_tps);
    if (d_gcp) cudaFree(d_gcp);
    if (d_geo) cudaFree(d_geo);
    return -1;
}

// GPU memory management (called from C++ code without CUDA includes)

int cudaUploadSource(const float* srcData, size_t bytes, float** outDevicePtr)
{
    if (cudaMalloc(outDevicePtr, bytes) != cudaSuccess) return -1;
    if (cudaMemcpy(*outDevicePtr, srcData, bytes, cudaMemcpyHostToDevice) != cudaSuccess)
    {
        cudaFree(*outDevicePtr);
        *outDevicePtr = nullptr;
        return -1;
    }
    return 0;
}

int cudaFreeDevicePtr(float* d_ptr)
{
    if (d_ptr) { cudaFree(d_ptr); return 0; }
    return -1;
}

int cudaAllocDevice(size_t bytes, float** outPtr)
{
    return (cudaMalloc(outPtr, bytes) == cudaSuccess) ? 0 : -1;
}

int cudaCopyFromDevice(float* hostDst, const float* d_src, size_t bytes)
{
    return (cudaMemcpy(hostDst, d_src, bytes, cudaMemcpyDeviceToHost) == cudaSuccess) ? 0 : -1;
}

// Device-pointer versions: srcData and dstData are already in GPU memory
// Caller is responsible for cudaMalloc/cudaMemcpy/cudaFree management

int cudaResampleTilePoly_device(
    const float* d_srcData, int srcRegionW, int srcRegionH, int bands,
    int srcRegionX, int srcRegionY,
    const double* polyCoeffs, int polyOrder,
    const double* outGeoTrans, int resampleMethod, float nodata,
    float* d_dstData, int dstX, int dstY, int dstW, int dstH)
{
    if (!cudaResampleAvailable()) return -1;

    int nTerm = (polyOrder + 1) * (polyOrder + 2) / 2;
    int coeffCount = 2 * nTerm;

    cudaError_t e = cudaMemcpyToSymbol(c_polyCoeffs, polyCoeffs,
        coeffCount * sizeof(double), 0, cudaMemcpyHostToDevice);
    if (e != cudaSuccess) return -1;

    double* d_geo = nullptr;
    if (cudaMalloc(&d_geo, 6 * sizeof(double)) != cudaSuccess) return -1;
    if (cudaMemcpy(d_geo, outGeoTrans, 6 * sizeof(double), cudaMemcpyHostToDevice) != cudaSuccess) {
        cudaFree(d_geo); return -1;
    }

    dim3 block(BLOCK_DIM, BLOCK_DIM);
    dim3 grid((dstW + BLOCK_DIM - 1) / BLOCK_DIM,
              (dstH + BLOCK_DIM - 1) / BLOCK_DIM);

    corrPolyKernel<<<grid, block>>>(
        d_srcData, srcRegionW, srcRegionH, bands,
        srcRegionX, srcRegionY,
        polyOrder, d_geo,
        resampleMethod, nodata,
        d_dstData, dstW, dstH);

    e = cudaGetLastError();
    cudaFree(d_geo);
    return (e == cudaSuccess) ? 0 : -1;
}

int cudaResampleTileTPS_device(
    const float* d_srcData, int srcRegionW, int srcRegionH, int bands,
    int srcRegionX, int srcRegionY,
    const double* tpsParams, const double* gcpSrcCoords, int numGcps,
    const double* outGeoTrans, int resampleMethod, float nodata,
    float* d_dstData, int dstX, int dstY, int dstW, int dstH)
{
    if (!cudaResampleAvailable()) return -1;

    int m = numGcps + 3;
    int paramCount = 2 * m;

    double *d_tps = nullptr, *d_gcp = nullptr, *d_geo = nullptr;

    if (cudaMalloc(&d_tps, paramCount * sizeof(double)) != cudaSuccess) return -1;
    if (cudaMalloc(&d_gcp, 2 * numGcps * sizeof(double)) != cudaSuccess) { cudaFree(d_tps); return -1; }
    if (cudaMalloc(&d_geo, 6 * sizeof(double)) != cudaSuccess) { cudaFree(d_tps); cudaFree(d_gcp); return -1; }

    if (cudaMemcpy(d_tps, tpsParams, paramCount * sizeof(double), cudaMemcpyHostToDevice) != cudaSuccess) goto fail2;
    if (cudaMemcpy(d_gcp, gcpSrcCoords, 2 * numGcps * sizeof(double), cudaMemcpyHostToDevice) != cudaSuccess) goto fail2;
    if (cudaMemcpy(d_geo, outGeoTrans, 6 * sizeof(double), cudaMemcpyHostToDevice) != cudaSuccess) goto fail2;

    {
        dim3 block(BLOCK_DIM, BLOCK_DIM);
        dim3 grid((dstW + BLOCK_DIM - 1) / BLOCK_DIM,
                  (dstH + BLOCK_DIM - 1) / BLOCK_DIM);

        corrTpsKernel<<<grid, block>>>(
            d_srcData, srcRegionW, srcRegionH, bands,
            srcRegionX, srcRegionY,
            d_tps, d_gcp, numGcps, d_geo,
            resampleMethod, nodata,
            d_dstData, dstW, dstH);
    }

    if (cudaGetLastError() != cudaSuccess) goto fail2;

    cudaFree(d_tps); cudaFree(d_gcp); cudaFree(d_geo);
    return 0;

fail2:
    cudaFree(d_tps); cudaFree(d_gcp); cudaFree(d_geo);
    return -1;
}

} // extern "C"

#endif // __INTELLISENSE__
