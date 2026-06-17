#include "HistogramMatcher.h"
#include <gdal_priv.h>
#include <algorithm>
#include <cmath>
#include <QDebug>

bool HistogramMatcher::match(const QString& refPath, const QString& targetPath,
                              const QString& outputPath)
{
    GDALAllRegister();
    GDALDataset* refDS = (GDALDataset*)GDALOpen(refPath.toUtf8(), GA_ReadOnly);
    GDALDataset* tgtDS = (GDALDataset*)GDALOpen(targetPath.toUtf8(), GA_ReadOnly);
    if (!refDS || !tgtDS)
    {
        if (refDS) GDALClose(refDS);
        if (tgtDS) GDALClose(tgtDS);
        qWarning() << "[HistogramMatcher] cannot open images";
        return false;
    }

    int bands  = std::min(refDS->GetRasterCount(), tgtDS->GetRasterCount());
    int tgtW   = tgtDS->GetRasterXSize(), tgtH = tgtDS->GetRasterYSize();
    int refW   = refDS->GetRasterXSize(), refH = refDS->GetRasterYSize();
    int refPixels = refW * refH;
    int tgtPixels = tgtW * tgtH;

    // 输出数据类型取参考影像与目标影像中范围更宽者,
    // 避免参考影像值域超出目标数据类型时发生截断导致偏亮/偏暗
    GDALDataType refDataType = refDS->GetRasterBand(1)->GetRasterDataType();
    GDALDataType tgtDataType = tgtDS->GetRasterBand(1)->GetRasterDataType();
    int refSize = GDALGetDataTypeSizeBytes(refDataType);
    int tgtSize = GDALGetDataTypeSizeBytes(tgtDataType);
    GDALDataType outDataType = (refSize >= tgtSize) ? refDataType : tgtDataType;
    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* outDS = drv->Create(outputPath.toUtf8(), tgtW, tgtH, bands,
                                      outDataType, nullptr);
    if (!outDS)
    {
        GDALClose(refDS); GDALClose(tgtDS);
        qWarning() << "[HistogramMatcher] cannot create output";
        return false;
    }
    double geo[6];
    tgtDS->GetGeoTransform(geo);
    outDS->SetGeoTransform(geo);
    outDS->SetProjection(tgtDS->GetProjectionRef());

    const int BLOCK_H = 256;
    const int BINS    = 65536;
    int maxW = std::max(refW, tgtW);
    float* blockBuf = new float[maxW * BLOCK_H];
    float* rowBuf   = new float[maxW];

    for (int b = 0; b < bands; ++b)
    {
        // 读取无效值标记, 用于跳过无效像元
        int bHasRefNodata = 0;
        float refNodata = 0.0f;
        refNodata = (float)refDS->GetRasterBand(b + 1)->GetNoDataValue(&bHasRefNodata);
        int bHasTgtNodata = 0;
        float tgtNodata = 0.0f;
        tgtNodata = (float)tgtDS->GetRasterBand(b + 1)->GetNoDataValue(&bHasTgtNodata);

        auto isRefNodata = [&](float v) { return bHasRefNodata && v == refNodata; };
        auto isTgtNodata = [&](float v) { return bHasTgtNodata && v == tgtNodata; };

        // ══════ 阶段1: 扫描 min/max (跳过无效值) ══════
        float tgtMin = 1e30f, tgtMax = -1e30f;
        float refMin = 1e30f, refMax = -1e30f;

        for (int y = 0; y < refH; y += BLOCK_H)
        {
            int h = std::min(BLOCK_H, refH - y);
            refDS->GetRasterBand(b + 1)->RasterIO(
                GF_Read, 0, y, refW, h,
                blockBuf, refW, h, GDT_Float32, 0, 0);
            int n = refW * h;
            for (int i = 0; i < n; ++i)
            {
                float v = blockBuf[i];
                if (isRefNodata(v)) continue;
                if (v < refMin) refMin = v;
                if (v > refMax) refMax = v;
            }
        }

        for (int y = 0; y < tgtH; y += BLOCK_H)
        {
            int h = std::min(BLOCK_H, tgtH - y);
            tgtDS->GetRasterBand(b + 1)->RasterIO(
                GF_Read, 0, y, tgtW, h,
                blockBuf, tgtW, h, GDT_Float32, 0, 0);
            int n = tgtW * h;
            for (int i = 0; i < n; ++i)
            {
                float v = blockBuf[i];
                if (isTgtNodata(v)) continue;
                if (v < tgtMin) tgtMin = v;
                if (v > tgtMax) tgtMax = v;
            }
        }

        if (refMin > refMax || tgtMin > tgtMax)
        {
            // 全是无效值或常量影像, 跳过该波段
            qWarning() << "[HistogramMatcher] band" << b << "has no valid data range, skipping";
            continue;
        }

        float refRange = refMax - refMin;
        float tgtRange = tgtMax - tgtMin;
        float refScale = (refRange > 1e-8f) ? (BINS - 1) / refRange : 1.0f;
        float tgtScale = (tgtRange > 1e-8f) ? (BINS - 1) / tgtRange : 1.0f;

        // ══════ 阶段2: 构建归一化直方图 (跳过无效值) ══════
        double* refHist = new double[BINS]{};
        double* tgtHist = new double[BINS]{};
        double refValidCount = 0.0;
        double tgtValidCount = 0.0;

        for (int y = 0; y < refH; y += BLOCK_H)
        {
            int h = std::min(BLOCK_H, refH - y);
            refDS->GetRasterBand(b + 1)->RasterIO(
                GF_Read, 0, y, refW, h,
                blockBuf, refW, h, GDT_Float32, 0, 0);
            int n = refW * h;
            for (int i = 0; i < n; ++i)
            {
                float v = blockBuf[i];
                if (isRefNodata(v)) continue;
                int idx = (int)((v - refMin) * refScale + 0.5f);
                if (idx < 0) idx = 0;
                if (idx >= BINS) idx = BINS - 1;
                refHist[idx]++;
                refValidCount++;
            }
        }

        for (int y = 0; y < tgtH; y += BLOCK_H)
        {
            int h = std::min(BLOCK_H, tgtH - y);
            tgtDS->GetRasterBand(b + 1)->RasterIO(
                GF_Read, 0, y, tgtW, h,
                blockBuf, tgtW, h, GDT_Float32, 0, 0);
            int n = tgtW * h;
            for (int i = 0; i < n; ++i)
            {
                float v = blockBuf[i];
                if (isTgtNodata(v)) continue;
                int idx = (int)((v - tgtMin) * tgtScale + 0.5f);
                if (idx < 0) idx = 0;
                if (idx >= BINS) idx = BINS - 1;
                tgtHist[idx]++;
                tgtValidCount++;
            }
        }

        // CDF → 归一化 (仅基于有效像元)
        for (int i = 1; i < BINS; ++i)
        {
            refHist[i] += refHist[i - 1];
            tgtHist[i] += tgtHist[i - 1];
        }
        double invRef = (refValidCount > 0) ? 1.0 / refValidCount : 1.0;
        double invTgt = (tgtValidCount > 0) ? 1.0 / tgtValidCount : 1.0;
        for (int i = 0; i < BINS; ++i)
        {
            refHist[i] *= invRef;
            tgtHist[i] *= invTgt;
        }

        // 建立查找表: 目标 bin → 参考影像实际像元值
        float* lut = new float[BINS];
        int rIdx = 0;
        for (int t = 0; t < BINS; ++t)
        {
            while (rIdx < BINS - 1 && refHist[rIdx] < tgtHist[t]) rIdx++;
            lut[t] = refMin + (float)rIdx / (BINS - 1) * refRange;
        }

        // ══════ 阶段3: 逐行应用 LUT (保留无效值) ══════
        for (int y = 0; y < tgtH; ++y)
        {
            tgtDS->GetRasterBand(b + 1)->RasterIO(
                GF_Read, 0, y, tgtW, 1,
                rowBuf, tgtW, 1, GDT_Float32, 0, 0);

            for (int x = 0; x < tgtW; ++x)
            {
                if (isTgtNodata(rowBuf[x])) continue;
                int idx = (int)((rowBuf[x] - tgtMin) * tgtScale + 0.5f);
                if (idx < 0) idx = 0;
                if (idx >= BINS) idx = BINS - 1;
                rowBuf[x] = lut[idx];
            }

            outDS->GetRasterBand(b + 1)->RasterIO(
                GF_Write, 0, y, tgtW, 1,
                rowBuf, tgtW, 1, GDT_Float32, 0, 0);
        }

        // 设置输出波段的无效值 (与目标影像一致)
        if (bHasTgtNodata)
            outDS->GetRasterBand(b + 1)->SetNoDataValue((double)tgtNodata);

        delete[] refHist; delete[] tgtHist; delete[] lut;
    }

    delete[] blockBuf;
    delete[] rowBuf;
    GDALClose(refDS); GDALClose(tgtDS); GDALClose(outDS);
    return true;
}
