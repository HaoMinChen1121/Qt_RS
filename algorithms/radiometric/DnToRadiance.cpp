#include "DnToRadiance.h"
#include "dataaccess/IRasterReader.h"
#include "dataaccess/IRasterWriter.h"
#include <QtMath>
#include <QDebug>

namespace Radiometric
{

AlgorithmResult DnToRadiance::process(IRasterReader* reader,
                                       IRasterWriter* writer,
                                       const RadiometricCorrectionParams& params,
                                       const SensorInfo& sensorInfo,
                                       ProgressCallback progress,
                                       int currentBand)
                                       {
    AlgorithmResult result;

    // ── 参数校验 ──
    if (!reader)
    {
        result.errorMessage = QStringLiteral("IRasterReader is null");
        return result;
    }
    if (!writer)
    {
        result.errorMessage = QStringLiteral("IRasterWriter is null");
        return result;
    }

    const int bandCount = reader->bandCount();
    if (bandCount == 0)
    {
        result.errorMessage = QStringLiteral("Source image has no bands");
        return result;
    }

    // ── 获取源影像元数据 ──
    const QSize rasterSize = reader->rasterSize();
    const int w = rasterSize.width();
    const int h = rasterSize.height();
    const QVector<double> geoTrans = reader->geoTransform();
    const QString projWkt = reader->projectionWkt();
    const double ndv = reader->noDataValue();
    const QString srcDataType = reader->dataType();

    // ── 确定增益/偏置来源 ──
    const bool autoMode = params.autoGainOffset;

    // ── 确定输出数据类型 ──
    QString outDataType = params.outputDataType.isEmpty()
        ? QStringLiteral("Float32") : params.outputDataType;

    // ── 构造输出文件路径 ──
    QString outPath = params.outputDirectory;
    if (!outPath.endsWith('/') && !outPath.endsWith('\\'))
    {
        outPath += '/';
    }
    outPath += QStringLiteral("radiance_") +
               (params.namingPattern.isEmpty() ? QStringLiteral("output.tif") : params.namingPattern);

    // ── 创建输出文件 ──
    if (!writer->create(outPath, w, h, bandCount, outDataType, geoTrans, projWkt, ndv))
    {
        result.errorMessage = QStringLiteral("Failed to create output file: ") + outPath;
        return result;
    }

    // ── 分块处理 ──
    const int blockSize = 512;
    const int xBlocks = (w + blockSize - 1) / blockSize;
    const int yBlocks = (h + blockSize - 1) / blockSize;
    const int totalBlocks = xBlocks * yBlocks * bandCount;

    int blockIdx = 0;

    // 单波段文件时用传入的波段号，多波段文件时 GDAL band index 即物理波段号
    int physBand = (bandCount == 1) ? currentBand : 1;

    for (int band = 1; band <= bandCount; ++band)
    {
        int lookupBand = physBand + (band - 1);

        // 获取该波段的增益和偏置
        double gain = params.manualGain;
        double offset = params.manualOffset;
        if (autoMode)
        {
            if (!sensorInfo.bandCalibration(lookupBand, gain, offset))
            {
                qWarning() << "[DnToRadiance] No calibration for physical band" << lookupBand
                           << "- using manual gain/offset";
                gain = params.manualGain;
                offset = params.manualOffset;
            }
        }

        for (int y = 0; y < h; y += blockSize)
        {
            for (int x = 0; x < w; x += blockSize)
            {
                int bw = std::min(blockSize, w - x);
                int bh = std::min(blockSize, h - y);

                // 读取当前分块
                QVector<float> data = reader->readBandWindow(band, x, y, bw, bh);
                if (data.isEmpty())
                {
                    writer->close();
                    result.errorMessage = QStringLiteral("Failed to read band %1 at (%2,%3)")
                        .arg(band).arg(x).arg(y);
                    return result;
                }

                // DN → Radiance: L = gain * DN + offset
                for (int i = 0; i < data.size(); ++i)
                {
                    if (data[i] != ndv)
                        data[i] = gain * data[i] + offset;
                }

                // 写入当前分块
                if (!writer->writeBandWindow(band, x, y, bw, bh, data))
                {
                    writer->close();
                    result.errorMessage = QStringLiteral("Failed to write band %1 at (%2,%3)")
                        .arg(band).arg(x).arg(y);
                    return result;
                }

                // 报告进度（返回 false 则用户请求取消）
                ++blockIdx;
                if (progress)
                {
                    int pct = blockIdx * 100 / totalBlocks;
                    if (!progress(pct, QStringLiteral("Radiance band %1/%2 [%3,%4]")
                        .arg(band).arg(bandCount).arg(x + bw).arg(y + bh)))
                        {
                        writer->close();
                        result.errorMessage = QStringLiteral("Cancelled by user");
                        return result;
                    }
                }
            }
        }
    }

    // ── 完成 ──
    writer->close();
    result.success = true;
    result.outputPath = outPath;
    return result;
}

} // namespace Radiometric
