#include "RadiometricWorker.h"
#include "algorithms/radiometric/DnToRadiance.h"
#include "algorithms/radiometric/DnToReflectance.h"
#include "algorithms/radiometric/AtmosphericCorrector.h"
#include "dataaccess/IRasterReader.h"
#include "dataaccess/IRasterWriter.h"
#include "dataaccess/ISensorProduct.h"
#include "dataaccess/SensorProductFactory.h"
#include "dataaccess/impl/DescriptorProduct.h"
#include "domain/ProductDescriptor.h"
#include <QDebug>
#include <QtMath>
#include <QDir>
#include <QMap>
#include <QRegularExpression>
#include <QFileInfo>

/// 从文件名提取物理波段号: *_B04.jp2 → 4,  *_B8A.jp2 → 9,  B01.tif → 1
static int extractPhysicalBand(const QString& filePath)
{
    QString base = QFileInfo(filePath).completeBaseName();
    // 匹配: Sentinel原始格式 _B04 或 输出格式 B04
    QRegularExpression rx(QStringLiteral("_?B(\\d+[A-Za-z]?)\\b"));
    QRegularExpressionMatch m = rx.match(base);
    if (m.hasMatch())
    {
        QString id = m.captured(1);
        if (id == QStringLiteral("8A")) return 9;
        if (id == QStringLiteral("8a")) return 9;
        bool ok = false;
        int n = id.toInt(&ok);
        if (ok && n >= 1 && n <= 12) return n;
    }
    return 1; // fallback
}

/// 在产物旁生成通用 .rpp 描述文件
static void generateDescriptor(const QString& outputDirPath,
                                const QString& sensorType,
                                const SensorInfo& sensorInfo,
                                const RadiometricCorrectionParams& params)
                                {
    if (outputDirPath.isEmpty() || !QFileInfo::exists(outputDirPath))
        return;

    QDir outDir(outputDirPath);

    // 扫描产物目录中的 TIFF 文件
    QStringList tifFiles = outDir.entryList({"*.tif", "*.tiff", "*.TIF", "*.TIFF"},
                                             QDir::Files, QDir::Name);

    // 无 TIFF → 尝试作为传感器产品打开（Sen2Cor 输出的 SAFE 目录等）
    if (tifFiles.isEmpty())
    {
        QScopedPointer<ISensorProduct> prod(createSensorProduct(outputDirPath));
        if (prod && prod->open(outputDirPath))
        {
            ProductDescriptor desc = DescriptorProduct::buildDescriptor(
                prod.data(), outputDirPath);
            QFileInfo fi(outputDirPath);
            for (auto& b : desc.bands)
                b.rasterPath = fi.dir().relativeFilePath(b.rasterPath);

            ProductDescriptor::ProcessingStep step;
            step.step      = QStringLiteral("radiometric");
            step.algorithm  = params.atmModel;
            step.timestamp  = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            desc.processingHistory.append(step);

            QString descPath = outputDirPath + ".rpp";
            if (desc.save(descPath))
                qDebug() << "[RadiometricWorker] descriptor saved:" << descPath;
        }
        else
        {
            qWarning() << "[RadiometricWorker] no output files found for descriptor";
        }
        return;
    }

    ProductDescriptor desc;
    desc.productId    = QFileInfo(outputDirPath).fileName();
    desc.sensorType   = sensorType;
    desc.generatedAt  = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    desc.originalPath = outputDirPath;

    // 传感器元数据
    desc.sensorInfoData.sensorId          = sensorInfo.sensorId;
    desc.sensorInfoData.acquisitionTime   = sensorInfo.acquisitionTime.toString(Qt::ISODate);
    desc.sensorInfoData.solarZenithAngle  = sensorInfo.solarZenithAngle;
    desc.sensorInfoData.solarAzimuthAngle = sensorInfo.solarAzimuthAngle;
    desc.sensorInfoData.earthSunDistance  = sensorInfo.earthSunDistance;
    desc.sensorInfoData.sensorZenithAngle  = sensorInfo.sensorZenithAngle;
    desc.sensorInfoData.sensorAzimuthAngle = sensorInfo.sensorAzimuthAngle;

    QString dirName = outDir.dirName(); // e.g. "temp"
    for (const QString& f : tifFiles)
    {
        ProductDescriptor::BandEntry be;
        be.bandName   = QFileInfo(f).completeBaseName();
        // .rpp is at <parent>/<dirname>.rpp, tif is in <parent>/<dirname>/f
        be.rasterPath = dirName + "/" + f;
        be.dataType   = QStringLiteral("Float32");

        // 从文件名提取物理波段号, 交叉 sensorInfo 补全元数据
        int pb = extractPhysicalBand(outDir.absoluteFilePath(f));
        be.physicalBand = pb;
        const SensorBandInfo* sbi = sensorInfo.findBand(
            QStringLiteral("B%1").arg(pb));
        if (sbi)
        {
            be.wavelengthMin     = sbi->wavelengthMin;
            be.wavelengthMax     = sbi->wavelengthMax;
            be.wavelengthCentral = sbi->wavelengthCentral;
            be.gain              = sbi->gain;
            be.offset            = sbi->offset;
            be.solarIrradiance   = sbi->solarIrradiance;
            be.resolution        = sbi->resolution;
            if (!sbi->bandName.isEmpty())
                be.bandName = sbi->bandName;
        }

        desc.bands.append(be);
    }

    ProductDescriptor::ProcessingStep step;
    step.step      = QStringLiteral("radiometric");
    step.algorithm  = params.atmModel;
    step.timestamp  = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    desc.processingHistory.append(step);

    QFileInfo fi(outputDirPath);
    QString descPath = fi.absolutePath() + "/" + fi.fileName() + ".rpp";
    if (desc.save(descPath))
        qDebug() << "[RadiometricWorker] descriptor saved:" << descPath;
}

RadiometricWorker::RadiometricWorker(IRasterReader* reader,
                                       IRasterWriter* writer,
                                       const RadiometricCorrectionParams& params,
                                       const SensorInfo& sensorInfo,
                                       QObject* parent)
    : TaskWorker(parent)
    , mReader(reader)
    , mWriter(writer)
    , mParams(params)
    , mSensorInfo(sensorInfo)
    {
}

void RadiometricWorker::process()
{
    QString calType = mParams.calibrationType;
    qDebug() << "[RadiometricWorker] process: type=" << calType
             << "atm=" << mParams.atmModel
             << "files=" << mParams.inputFiles.size();

    // ── 确定是否做大气校正（提前判断，用于目录规划）──
    const bool doAtmCorrection = (mParams.atmModel != QStringLiteral("None")
                                  && !mParams.atmModel.isEmpty()
                                  && mParams.atmModel != QStringLiteral("Sen2Cor"));

    // ── 确定工作目录 ──
    QString workDir = mParams.outputDirectory;
    if (workDir.isEmpty())
        workDir = QDir::currentPath();
    QDir().mkpath(workDir);

    // TOA 中间产物目录（仅当需要后续大气校正时使用）
    QString tempDir;
    if (doAtmCorrection)
    {
        tempDir = workDir + QStringLiteral("/toa_tmp");
        QDir().mkpath(tempDir);
    }
    else
    {
        tempDir = workDir;  // 无大气校正 → 直接输出到 workDir
    }

    // ── 输入文件校验 ──
    QStringList files = mParams.inputFiles;
    if (files.isEmpty())
    {
        emit errorOccurred(QStringLiteral("No input files specified"));
        emit finished(false, QString());
        return;
    }

    // ── 构造进度回调：包装 isCancelled() 检查 ──
    ProgressCallback progressFn = [this](int percent, const QString& msg) -> bool
    {
        if (isCancelled())
        {
            emit errorOccurred(QStringLiteral("Cancelled"));
            return false;
        }
        emit progressChanged(percent, msg);
        return true;
    };

    // ── 输出数据类型 ──
    QString outDataType = mParams.outputDataType.isEmpty()
        ? QStringLiteral("Float32") : mParams.outputDataType;

    // ══════════════════════════════════════════════════════════════════════════
    // Sen2Cor 模式：全目录处理，不走逐波段流程
    // ══════════════════════════════════════════════════════════════════════════
    if (mParams.atmModel == QStringLiteral("Sen2Cor"))
    {
        QString inputPath = files.first();
        emit progressChanged(5, QStringLiteral("启动 Sen2Cor 大气校正..."));
        Radiometric::AtmosphericCorrector atm;
        AlgorithmResult atmResult = atm.runSen2Cor(inputPath, mParams, workDir,
            [this](int pct, const QString& msg)
            {
                if (isCancelled()) return false;
                emit progressChanged(5 + pct * 90 / 100, msg);
                return true;
            });

        if (!atmResult.success)
        {
            emit errorOccurred(atmResult.errorMessage);
            emit finished(false, QString());
        }
        else
        {
            emit progressChanged(98, QStringLiteral("Sen2Cor 完成，写入产物..."));
            generateDescriptor(atmResult.outputPath, mParams.sensorType,
                               mSensorInfo, mParams);
            emit finished(true, atmResult.outputPath);
        }
        return;
    }

    // ══════════════════════════════════════════════════════════════════════════
    // 6S / Py6S / None 模式：Phase 1 — 运行大气模型，获取全波段校正系数
    //
    // 大气校正支持两条路线，由用户选择的辐射定标类型决定：
    //   DN2Radiance   → 辐亮度路线：6S 输出 xa（辐亮度版系数）
    //   DN2Reflectance → 反射率路线：6S 输出 xap（反射率版系数）
    // ══════════════════════════════════════════════════════════════════════════

    QVector<Radiometric::AtmosphericCoefficients> allCoefs;
    QMap<int, int> bandToCoef; // physBand → index in allCoefs

    if (doAtmCorrection)
    {
        qDebug() << "[RadiometricWorker] SensorInfo bands:" << mSensorInfo.bands.size()
                 << "atmModel:" << mParams.atmModel
                 << "calType:" << calType
                 << "inputFiles:" << mParams.inputFiles.size();

        int validBands = 0;
        for (const auto& b : mSensorInfo.bands)
            if (b.wavelengthMin > 0.0 && b.wavelengthMax > 0.0) ++validBands;
        qDebug() << "[RadiometricWorker] Valid bands (wl>0):" << validBands;

        Radiometric::AtmosphericCorrector atm;
        allCoefs = atm.computeCorrectionCoefficients(mParams, mSensorInfo,
                                                      workDir, progressFn);
        if (allCoefs.isEmpty())
        {
            emit errorOccurred(QStringLiteral(
                "Failed to compute atmospheric correction coefficients: ") + mParams.atmModel);
            emit finished(false, QString());
            return;
        }

        for (int i = 0; i < allCoefs.size(); ++i)
            bandToCoef.insert(allCoefs[i].bandIndex, i);

        if (progressFn)
            progressFn(70, QStringLiteral("Applying atmospheric correction per band..."));
    }

    // ══════════════════════════════════════════════════════════════════════════
    // Phase 2：逐文件处理 — DN→TOA → 大气校正
    // ══════════════════════════════════════════════════════════════════════════
    for (int f = 0; f < files.size(); ++f)
    {
        if (isCancelled())
        {
            emit errorOccurred(QStringLiteral("Cancelled"));
            emit finished(false, QString());
            return;
        }

        const QString& inputFile = files[f];
        int physBand = extractPhysicalBand(inputFile);

        // ── 打开源文件 ──
        if (!mReader->open(inputFile))
        {
            emit errorOccurred(QStringLiteral("Failed to open: ") + inputFile);
            emit finished(false, QString());
            return;
        }

        SensorInfo info = mSensorInfo;
        const int srcBandCount = mReader->bandCount();

        // ── 使用逐文件唯一的输出文件名 ──
        RadiometricCorrectionParams fileParams = mParams;
        fileParams.outputDirectory = tempDir;
        // Clean naming: B01.tif, B02.tif, ...
        fileParams.namingPattern = QStringLiteral("B%1.tif")
            .arg(physBand, 2, 10, QChar('0'));

        AlgorithmResult result;

        // ── 步骤 1：DN → TOA 反射率（中间产物） ──
        if (calType == QStringLiteral("DN2Radiance"))
        {
            Radiometric::DnToRadiance algo;
            result = algo.process(mReader, mWriter, fileParams, info, progressFn, physBand);
        }
        else // DN2Reflectance
        {
            Radiometric::DnToReflectance algo;
            result = algo.process(mReader, mWriter, fileParams, info, progressFn, physBand);
        }

        mReader->close();
        mWriter->close();

        if (!result.success)
        {
            emit errorOccurred(result.errorMessage);
            emit finished(false, QString());
            return;
        }

        QString toaFile = result.outputPath;

        // ── 步骤 2：大气校正（6S / Py6S）——逐波段 ──
        if (doAtmCorrection)
        {
            // 重新打开 TOA 中间文件作为输入
            if (!mReader->open(toaFile))
            {
                emit errorOccurred(QStringLiteral("Failed to reopen TOA file: ") + toaFile);
                emit finished(false, QString());
                return;
            }

            const QSize sz = mReader->rasterSize();
            const QVector<double> geo = mReader->geoTransform();
            const QString proj = mReader->projectionWkt();
            const double ndv = mReader->noDataValue();
            const int toaBandCount = mReader->bandCount();

            // 创建大气校正后的最终输出文件（直接输出到 workDir）
            QString finalPath = workDir + QStringLiteral("/B%1.tif")
                                .arg(physBand, 2, 10, QChar('0'));
            if (!mWriter->create(finalPath, sz.width(), sz.height(), toaBandCount,
                                 outDataType, geo, proj, ndv))
                                 {
                mReader->close();
                emit errorOccurred(QStringLiteral("Failed to create output: ") + finalPath);
                emit finished(false, QString());
                return;
            }

            Radiometric::AtmosphericCorrector atm;

            if (toaBandCount == 1)
            {
                // 单波段文件：直接匹配物理波段号
                int coefIdx = bandToCoef.value(physBand, -1);
                if (coefIdx < 0)
                {
                    qWarning() << "[RadiometricWorker] No coefficients for band"
                              << physBand << ", skipping correction";
                    mReader->close();
                    mWriter->close();
                    continue;
                }

                AlgorithmResult corrResult = atm.applyCorrection(
                    mReader, mWriter, 1, allCoefs[coefIdx], progressFn);

                if (!corrResult.success)
                {
                    mReader->close();
                    mWriter->close();
                    emit errorOccurred(corrResult.errorMessage);
                    emit finished(false, QString());
                    return;
                }
            }
            else
            {
                // 多波段文件：按 GDAL band 顺序匹配系数索引
                qDebug() << "[RadiometricWorker] Multi-band TOA file with"
                         << toaBandCount << "bands";

                for (int b = 1; b <= toaBandCount; ++b)
                {
                    if (isCancelled())
                    {
                        mReader->close();
                        mWriter->close();
                        emit errorOccurred(QStringLiteral("Cancelled"));
                        emit finished(false, QString());
                        return;
                    }

                    int coefIdx = b - 1;
                    if (coefIdx >= allCoefs.size())
                    {
                        qWarning() << "[RadiometricWorker] No coefficient for band index"
                                  << b << ", skipping";
                        continue;
                    }

                    AlgorithmResult corrResult = atm.applyCorrection(
                        mReader, mWriter, b, allCoefs[coefIdx], progressFn);

                    if (!corrResult.success)
                    {
                        mReader->close();
                        mWriter->close();
                        emit errorOccurred(corrResult.errorMessage);
                        emit finished(false, QString());
                        return;
                    }
                }
            }

            mReader->close();
            mWriter->close();
            qDebug() << "[RadiometricWorker] Atmospheric correction done:" << finalPath;
        }
    }

    // None 模式的最终产物在 tempDir，其余模式在 workDir
    QString descDir = doAtmCorrection ? workDir : tempDir;
    generateDescriptor(descDir, mParams.sensorType,
                       mSensorInfo, mParams);
    emit finished(true, descDir);
}
