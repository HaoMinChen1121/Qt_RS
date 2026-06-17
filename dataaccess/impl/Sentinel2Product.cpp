#include "Sentinel2Product.h"
#include <QFileInfo>
#include <QDir>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QDebug>
#include <cmath>
#include <gdal_priv.h>
#include <cpl_vsi.h>
#include <ogr_srs_api.h>

// ─── helpers ────────────────────────────────────────────────────────────────

/// QFileInfo 无法识别 /vsizip/ 虚拟路径，用 VSI 检查文件是否存在
static bool vsiExists(const QString& path)
{
    VSILFILE* fp = VSIFOpenExL(path.toUtf8().constData(), "rb", FALSE);
    if (fp) { VSIFCloseL(fp); return true; }
    return false;
}

QByteArray Sentinel2Product::readTextFile(const QString& vsiPath)
{
    VSILFILE* fp = VSIFOpenExL(vsiPath.toUtf8().constData(), "rb", FALSE);
    if (!fp)
    {
        qWarning() << "[Sentinel2Product] VSIFOpenExL failed:" << vsiPath;
        return {};
    }
    VSIFSeekL(fp, 0, SEEK_END);
    long size = VSIFTellL(fp);
    VSIFSeekL(fp, 0, SEEK_SET);
    QByteArray data(size, Qt::Uninitialized);
    VSIFReadL(data.data(), 1, static_cast<size_t>(size), fp);
    VSIFCloseL(fp);
    return data;
}

QSize Sentinel2Product::probeRasterSize(const QString& rasterPath)
{
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALDataset* ds = static_cast<GDALDataset*>(
        GDALOpen(rasterPath.toUtf8().constData(), GA_ReadOnly));
    CPLPopErrorHandler();
    if (!ds)
        return {};
    QSize sz(ds->GetRasterXSize(), ds->GetRasterYSize());
    GDALClose(ds);
    return sz;
}

// ─── open / close ───────────────────────────────────────────────────────────

bool Sentinel2Product::open(const QString& path)
{
    close();

    QFileInfo fi(path);
    if (!fi.exists())
    {
        qWarning() << "[Sentinel2Product] path does not exist:" << path;
        return false;
    }

    mOriginalPath = fi.absoluteFilePath();

    if (path.endsWith(".zip", Qt::CaseInsensitive))
    {
        return openFromZip(mOriginalPath);
    } else if (path.endsWith(".SAFE", Qt::CaseInsensitive))
    {
        return openFromSafeDir(mOriginalPath);
    }

    // 也可能是 .SAFE 目录没有后缀，尝试查找 manifest.safe
    QString testManifest = mOriginalPath + "/manifest.safe";
    if (QFileInfo::exists(testManifest))
        return openFromSafeDir(mOriginalPath);

    qWarning() << "[Sentinel2Product] unrecognized Sentinel-2 product:" << path;
    return false;
}

void Sentinel2Product::close()
{
    mBands.clear();
    mSensorInfo  = SensorInfo();
    mRootPath.clear();
    mSafeDirName.clear();
    mPlatform.clear();
    mTileId.clear();
    mGranuleDirName.clear();
    mOpen = false;
}

bool Sentinel2Product::isOpen() const
{
    return mOpen;
}

// ─── path helpers ───────────────────────────────────────────────────────────

static QString s2Platform(const QString& safeName)
{
    if (safeName.startsWith("S2A_"))
        return QStringLiteral("Sentinel-2A");
    if (safeName.startsWith("S2B_"))
        return QStringLiteral("Sentinel-2B");
    if (safeName.startsWith("S2C_"))
        return QStringLiteral("Sentinel-2C");
    return QStringLiteral("Sentinel-2");
}

// ─── ZIP open ───────────────────────────────────────────────────────────────

bool Sentinel2Product::openFromZip(const QString& zipPath)
{
    QFileInfo fi(zipPath);
    mSafeDirName = fi.completeBaseName(); // stem without .zip → S2B_MSIL1C_....SAFE

    // 构建 /vsizip/ 根路径
    mRootPath = QStringLiteral("/vsizip/") + zipPath + QStringLiteral("/") + mSafeDirName + QStringLiteral(".SAFE");

    // 读取 manifest.safe 以发现波段文件
    QString manifestPath = mRootPath + QStringLiteral("/manifest.safe");
    if (!readManifestXml(manifestPath))
    {
        qWarning() << "[Sentinel2Product] failed to read manifest.safe from ZIP";
        return false;
    }

    // 解析 MTD XML: L1C 用 MTD_MSIL1C.xml，L2A 用 MTD_MSIL2A.xml
    // QFileInfo 无法识别 /vsizip/，必须用 VSI 检查存在性
    QString xmlPath = mRootPath + QStringLiteral("/MTD_MSIL1C.xml");
    if (!vsiExists(xmlPath))
    {
        xmlPath = mRootPath + QStringLiteral("/MTD_MSIL2A.xml");
        if (!vsiExists(xmlPath))
        {
            // 最后回退 S2A_OPER_MTD_SAFL2A.xml (某些旧命名)
            QString fallback = mRootPath + QStringLiteral("/S2A_OPER_MTD_SAFL2A.xml");
            if (vsiExists(fallback))
                xmlPath = fallback;
        }
    }
    mSensorInfo = parseMtdMsil1cXml(xmlPath);
    mPlatform   = s2Platform(mSafeDirName);
    mSensorInfo.sensorType = mPlatform;
    mSensorInfo.sensorId   = mPlatform;
    mSensorInfo.tileId     = mTileId;

    // Baseline >= 05.00 把太阳角度移到了 Tile 级 MTD_TL.xml
    if (!mGranuleDirName.isEmpty())
    {
        QString tlPath = mRootPath + QStringLiteral("/") + mGranuleDirName + QStringLiteral("/MTD_TL.xml");
        parseMtdTlSunAngles(tlPath, mSensorInfo);
    }

    mOpen = true;
    qDebug() << "[Sentinel2Product] opened ZIP product:" << mSafeDirName
             << "bands:" << mBands.size()
             << "platform:" << mPlatform;
    return true;
}

// ─── extracted SAFE directory open ──────────────────────────────────────────

bool Sentinel2Product::openFromSafeDir(const QString& safeDirPath)
{
    QDir dir(safeDirPath);
    mSafeDirName = dir.dirName();
    mRootPath    = safeDirPath;

    GDALAllRegister(); // 确保 GDAL VSI 层就绪
    QString manifestPath = mRootPath + QStringLiteral("/manifest.safe");
    qDebug() << "[Sentinel2Product] opening SAFE dir, manifest:" << manifestPath;
    if (!readManifestXml(manifestPath))
    {
        qWarning() << "[Sentinel2Product] failed to read manifest.safe from SAFE dir";
        return false;
    }

    QString xmlPath = mRootPath + QStringLiteral("/MTD_MSIL1C.xml");
    if (!QFileInfo::exists(xmlPath))
    {
        xmlPath = mRootPath + QStringLiteral("/MTD_MSIL2A.xml");
        if (!QFileInfo::exists(xmlPath))
            xmlPath = mRootPath + QStringLiteral("/S2A_OPER_MTD_SAFL2A.xml");
    }
    mSensorInfo = parseMtdMsil1cXml(xmlPath);
    mPlatform   = s2Platform(mSafeDirName);
    mSensorInfo.sensorType = mPlatform;
    mSensorInfo.sensorId   = mPlatform;
    mSensorInfo.tileId     = mTileId;

    if (!mGranuleDirName.isEmpty())
    {
        QString tlPath = mRootPath + QStringLiteral("/") + mGranuleDirName + QStringLiteral("/MTD_TL.xml");
        parseMtdTlSunAngles(tlPath, mSensorInfo);
    }

    mOpen = true;
    qDebug() << "[Sentinel2Product] opened SAFE product:" << mSafeDirName
             << "bands:" << mBands.size();
    return true;
}

// ─── manifest.safe parsing ──────────────────────────────────────────────────

bool Sentinel2Product::readManifestXml(const QString& manifestPath)
{
    GDALAllRegister();
    QByteArray xmlData = readTextFile(manifestPath);
    if (xmlData.isEmpty())
    {
        qWarning() << "[Sentinel2Product] readTextFile empty, path:" << manifestPath
                   << "size:" << QFileInfo(manifestPath).size();
        return false;
    }

    QXmlStreamReader xml(xmlData);
    QStringList jp2Paths;

    while (!xml.atEnd() && !xml.hasError())
    {
        QXmlStreamReader::TokenType t = xml.readNext();
        if (t == QXmlStreamReader::StartElement && xml.name() == QStringLiteral("fileLocation"))
        {
            QString href = xml.attributes().value("href").toString();
            if (href.endsWith(".jp2", Qt::CaseInsensitive))
            {
                // 去掉相对路径前缀 "./"
                if (href.startsWith("./"))
                    href = href.mid(2);
                jp2Paths.append(href);
            }
        }
    }

    if (xml.hasError())
    {
        qWarning() << "[Sentinel2Product] XML error in manifest.safe:" << xml.errorString();
    }

    if (jp2Paths.isEmpty())
    {
        qWarning() << "[Sentinel2Product] no JP2 files found in manifest";
        return false;
    }

    qDebug() << "[Sentinel2Product] found" << jp2Paths.size() << "JP2 paths in manifest, first 5:";
    for (int i = 0; i < qMin(5, jp2Paths.size()); ++i)
        qDebug() << "  " << jp2Paths[i];

    // 构建波段描述符
    mBands.clear();
    mTileId.clear();
    mGranuleDirName.clear();

    // 波段名 → 物理编号映射
    static const QMap<QString, QPair<int, QString>> bandMap = {
        { "B01", {1,  "Coastal/Aerosol"}},
        { "B02", {2,  "Blue"}},
        { "B03", {3,  "Green"}},
        { "B04", {4,  "Red"}},
        { "B05", {5,  "Red-Edge-1"}},
        { "B06", {6,  "Red-Edge-2"}},
        { "B07", {7,  "Red-Edge-3"}},
        { "B08", {8,  "NIR-Broad"}},
        { "B8A", {9,  "NIR-Narrow"}}, // B8A → bandNumber 9
        { "B09", {10, "Water Vapour"}},
        { "B10", {11, "SWIR-Cirrus"}},
        { "B11", {12, "SWIR-1"}},
        { "B12", {13, "SWIR-2"}},
    };

    // S2 各波段的默认分辨率 (米)
    static const QMap<QString, double> resolutionMap = {
        { "B01", 60}, { "B02", 10}, { "B03", 10}, { "B04", 10},
        { "B05", 20}, { "B06", 20}, { "B07", 20}, { "B08", 10},
        { "B8A", 20}, { "B09", 60}, { "B10", 60}, { "B11", 20}, { "B12", 20},
    };

    // 用于 L2A 重复波段的去重：同一 physicalBand 只保留最高分辨率 (最小米数)
    QMap<int, int> bandBestIdx;  // physicalBand → mBands index

    for (const QString& relPath : jp2Paths)
    {
        // 统一路径分隔符为 '/'
        QString normPath = relPath;
        normPath.replace(QLatin1Char('\\'), QLatin1Char('/'));

        // 只处理 IMG_DATA 目录下的真波段，跳过 QI_DATA 掩膜
        if (!normPath.contains("/IMG_DATA/", Qt::CaseInsensitive))
            continue;

        QString baseName = QFileInfo(normPath).completeBaseName();

        // 提取波段后缀 (L1C: T49RFN_20240705T030529_B04; L2A: …_B04_10m)
        static const QRegularExpression bandIdRx(QStringLiteral("_(B\\d{1,2}[A-Za-z]?)(?:_\\d{1,2}m)?$"));
        QString bandSuffix;
        QRegularExpressionMatch bandMatch = bandIdRx.match(baseName);
        if (bandMatch.hasMatch())
            bandSuffix = bandMatch.captured(1);

        if (bandSuffix.isEmpty() || bandSuffix == "TCI")
        {
            qDebug() << "[Sentinel2Product] skipping non-band:" << baseName << "suffix:" << bandSuffix;
            continue;
        }

        auto it = bandMap.find(bandSuffix);
        if (it == bandMap.end())
        {
            qDebug() << "[Sentinel2Product] unknown band suffix:" << bandSuffix << "from:" << baseName;
            continue;
        }

        // 从文件名或目录提取分辨率 (L2A: R10m / _10m; L1C: fallback 查表)
        double fileResolution = resolutionMap.value(bandSuffix, 0.0);
        static const QRegularExpression resRx(QStringLiteral("[R_](\\d{1,2})m"));
        QRegularExpressionMatch resMatch = resRx.match(normPath);
        if (resMatch.hasMatch())
        {
            bool ok = false;
            double r = resMatch.captured(1).toDouble(&ok);
            if (ok && r > 0)
                fileResolution = r;
        }

        // 推导 tile ID 和 granule 目录 (只做一次)
        if (mTileId.isEmpty())
        {
            int slashAfterT = baseName.indexOf('_');
            if (slashAfterT < 0)
                slashAfterT = baseName.size();
            mTileId = baseName.left(slashAfterT);

            int granuleEnd = normPath.indexOf("/IMG_DATA");
            if (granuleEnd >= 0)
                mGranuleDirName = normPath.left(granuleEnd);
        }

        QString fullPath = mRootPath + QStringLiteral("/") + normPath;

        RasterBandDescriptor desc;
        desc.physicalBand = it->first;
        desc.bandName     = it->second;
        desc.rasterPath   = fullPath;
        desc.resolution   = fileResolution;
        desc.dataType     = QStringLiteral("UInt16");

        // L2A 同一波段可能出现在多个分辨率目录，优先保留最高分辨率 (最小米数)
        int physBand = it->first;
        if (bandBestIdx.contains(physBand))
        {
            int prevIdx = bandBestIdx[physBand];
            if (fileResolution < mBands[prevIdx].resolution)
                mBands[prevIdx] = desc;
        }
        else
        {
            bandBestIdx.insert(physBand, mBands.size());
            mBands.append(desc);
        }
    }

    if (mBands.isEmpty())
    {
        qWarning() << "[Sentinel2Product] all JP2 files filtered out, check band naming";
        return false;
    }

    // 按物理波段号排序
    std::sort(mBands.begin(), mBands.end(),
              [](const RasterBandDescriptor& a, const RasterBandDescriptor& b)
              {
                  return a.physicalBand < b.physicalBand;
              });

    return true;
}

// ─── MTD_MSIL1C.xml parsing ─────────────────────────────────────────────────

SensorInfo Sentinel2Product::parseMtdMsil1cXml(const QString& xmlPath) const
{
    SensorInfo info;
    info.quantificationValue = 10000.0;
    info.reflectanceU        = 1.0;

    QByteArray xmlData = readTextFile(xmlPath);
    if (xmlData.isEmpty())
        return info;

    QXmlStreamReader xml(xmlData);

    // 收集所有叶子路径 + 直接捕获太阳角度（避免 stack 路径错乱问题）
    QMap<QString, QString> leaf;
    QStringList stack;
    bool inMeanSunAngle = false;
    bool inMeanViewingAngle = false;
    QString pendingAngleElement;   // 当前等待文本值的 SUN 角度元素名
    QString pendingViewingElement; // 当前等待文本值的 VIEW 角度元素名

    while (!xml.atEnd() && !xml.hasError())
    {
        switch (xml.readNext())
        {
        case QXmlStreamReader::StartElement: {
            QString name = xml.name().toString();
            stack.append(name);

            if (name == QStringLiteral("Mean_Sun_Angle"))
                inMeanSunAngle = true;
            else if (name == QStringLiteral("Mean_Viewing_Incidence_Angle"))
                inMeanViewingAngle = true;
            else if (inMeanSunAngle && (name == QStringLiteral("ZENITH_ANGLE")
                                        || name == QStringLiteral("AZIMUTH_ANGLE")))
                pendingAngleElement = name;
            else if (inMeanViewingAngle && (name == QStringLiteral("ZENITH_ANGLE")
                                            || name == QStringLiteral("AZIMUTH_ANGLE")))
                pendingViewingElement = name;

            break;
        }
        case QXmlStreamReader::EndElement: {
            QString name = xml.name().toString();
            if (!stack.isEmpty())
                stack.removeLast();
            if (name == QStringLiteral("Mean_Sun_Angle"))
                inMeanSunAngle = false;
            else if (name == QStringLiteral("Mean_Viewing_Incidence_Angle"))
                inMeanViewingAngle = false;
            break;
        }
        case QXmlStreamReader::Characters: {
            QString text = xml.text().toString().trimmed();
            if (text.isEmpty())
                break;

            // 优先：直接捕获太阳角度值
            if (!pendingAngleElement.isEmpty())
            {
                leaf.insert(QStringLiteral("SUN_") + pendingAngleElement, text);
                pendingAngleElement.clear();
                break;
            }

            // 直接捕获观测角度值
            if (!pendingViewingElement.isEmpty())
            {
                leaf.insert(QStringLiteral("VIEW_") + pendingViewingElement, text);
                pendingViewingElement.clear();
                break;
            }

            if (!stack.isEmpty())
                leaf.insert(stack.join('/'), text);
            break;
        }
        default:
            break;
        }
    }

    if (xml.hasError())
        qWarning() << "[Sentinel2Product] XML parse error in" << xmlPath << ":" << xml.errorString()
                   << "leaf entries:" << leaf.size();
    else
        qDebug() << "[Sentinel2Product] parsed" << leaf.size() << "leaf entries from" << xmlPath;

    auto find = [&](const QString& key, const QString& preferParent = {}) -> QString {
        QString fallback;
        for (auto it = leaf.begin(); it != leaf.end(); ++it)
        {
            if (!it.key().endsWith(key))
                continue;
            if (!preferParent.isEmpty() && it.key().contains(preferParent))
                return it.value();
            if (fallback.isEmpty())
                fallback = it.value();
        }
        return fallback;
    };

    // ── 采集时间 ──
    // MTD_MSIL1C.xml 中字段名为 PRODUCT_START_TIME（位于 Product_Info 下），
    // 而非 SENSING_TIME（后者仅存在于 Tile 级 MTD_TL.xml）
    QString sensingTime = find("SENSING_TIME");
    if (sensingTime.isEmpty())
        sensingTime = find("PRODUCT_START_TIME");
    if (!sensingTime.isEmpty())
        info.acquisitionTime = QDateTime::fromString(sensingTime, Qt::ISODate);

    // ── 产品识别 ──
    info.sensorId = find("SPACECRAFT_NAME"); // "Sentinel-2B"

    // ── 太阳/传感器角度 ──
    // 直接捕获自 Mean_Sun_Angle 容器内，key 前缀为 "SUN_" 避免与观测角混淆
    info.solarZenithAngle  = leaf.value("SUN_ZENITH_ANGLE").toDouble();
    info.solarAzimuthAngle = leaf.value("SUN_AZIMUTH_ANGLE").toDouble();

    // 直接捕获自 Mean_Viewing_Incidence_Angle 容器内，key 前缀为 "VIEW_"
    info.sensorZenithAngle  = leaf.value("VIEW_ZENITH_ANGLE").toDouble();
    info.sensorAzimuthAngle = leaf.value("VIEW_AZIMUTH_ANGLE").toDouble();

    // ── 日地距离 (AU) ──
    // Sentinel-2 XML 不直接存储，由采集日期的年积日天文公式推算
    if (info.acquisitionTime.isValid())
    {
        int doy = info.acquisitionTime.date().dayOfYear();
        info.earthSunDistance = 1.0 / std::sqrt(1.0 - 0.01673 * std::cos(2.0 * M_PI * (doy - 3) / 365.0));
    }

    // ── 特殊值 ──
    // NODATA = 0, SATURATED = 65535 (L1C 标准)

    // ── 量化值 ──
    QString qv = find("QUANTIFICATION_VALUE");
    if (!qv.isEmpty())
        info.quantificationValue = qv.toDouble();

    // ── 反射率 U 因子 ──
    QString uFactor = find("U");
    if (!uFactor.isEmpty())
        info.reflectanceU = uFactor.toDouble();

    // ── 波段参数: RADIO_ADD_OFFSET, PHYSICAL_GAINS, SOLAR_IRRADIANCE ──
    // 需要从 XML 属性中提取 band_id，QMap 字母序迭代会丢失顺序
    QMap<int, double> radioOffsets;
    QMap<int, double> physicalGains;
    QMap<int, double> irradiances;
    {
        xml.clear();
        xml.addData(xmlData);
        while (!xml.atEnd() && !xml.hasError())
        {
            QXmlStreamReader::TokenType t = xml.readNext();
            if (t == QXmlStreamReader::StartElement)
            {
                QString name = xml.name().toString();
                QStringRef bidRef = xml.attributes().value("band_id");
                if (bidRef.isEmpty())
                    bidRef = xml.attributes().value("bandId");
                if (!bidRef.isEmpty())
                {
                    int bid = bidRef.toString().toInt();
                    if (name == "RADIO_ADD_OFFSET")
                        radioOffsets.insert(bid, xml.readElementText().toDouble());
                    else if (name == "PHYSICAL_GAINS")
                        physicalGains.insert(bid, xml.readElementText().toDouble());
                    else if (name == "SOLAR_IRRADIANCE")
                        irradiances.insert(bid, xml.readElementText().toDouble());
                }
            }
        }
    }

    // ── 波段: Spectral_Information ──
    // 波段定义数组 (bandId → 物理波段映射)
    struct RawBand
    {
        int    bandId = -1;
        QString physicalBand;
        double resolution = 0;
        double wlMin = 0, wlMax = 0, wlCentral = 0;
    };
    QList<RawBand> rawBands;

    // 重新解析，提取完整的 Spectral_Information
    xml.clear();
    xml.addData(xmlData);
    QString spectralBandId;
    while (!xml.atEnd() && !xml.hasError())
    {
        switch (xml.readNext())
        {
        case QXmlStreamReader::StartElement: {
            QString name = xml.name().toString();
            if (name == "Spectral_Information")
                spectralBandId = xml.attributes().value("bandId").toString();
            else if (name == "RESOLUTION" && !spectralBandId.isEmpty())
            {
                RawBand rb;
                rb.bandId     = spectralBandId.toInt();
                rb.resolution = xml.readElementText().toDouble();
                rawBands.append(rb);
                spectralBandId.clear(); // 下一个 Spectral_Information
            }
            break;
        }
        default:
            break;
        }
    }

    // 重新解析，提取 Wavelength 和 Spectral_Information.physicalBand
    xml.clear();
    xml.addData(xmlData);
    int currentRawIdx = -1;
    bool inWavelength = false;  // 是否在 Wavelength 元素内
    double pendingWlMin = 0, pendingWlMax = 0, pendingWlCentral = 0;
    while (!xml.atEnd() && !xml.hasError())
    {
        switch (xml.readNext())
        {
        case QXmlStreamReader::StartElement: {
            QString name = xml.name().toString();
            if (name == "Spectral_Information")
            {
                currentRawIdx++;
                QString physBand = xml.attributes().value("physicalBand").toString();
                if (currentRawIdx < rawBands.size())
                    rawBands[currentRawIdx].physicalBand = physBand;
            }
            else if (name == "Wavelength")
            {
                inWavelength = true;
                pendingWlMin = pendingWlMax = pendingWlCentral = 0;
            }
            else if (name == "MIN" && inWavelength)
            {
                pendingWlMin = xml.readElementText().toDouble();
            }
            else if (name == "MAX" && inWavelength)
            {
                pendingWlMax = xml.readElementText().toDouble();
            }
            else if (name == "CENTRAL" && inWavelength)
            {
                pendingWlCentral = xml.readElementText().toDouble();
            }
            break;
        }
        case QXmlStreamReader::EndElement: {
            QString name = xml.name().toString();
            if (name == "Wavelength")
            {
                inWavelength = false;
                if (currentRawIdx >= 0 && currentRawIdx < rawBands.size())
                {
                    // Sentinel-2 XML 部分基线版本使用纳米，6S/Py6S 需要微米
                    auto toMicrons = [](double v) {
                        return (v > 100.0) ? v / 1000.0 : v;
                    };
                    rawBands[currentRawIdx].wlMin    = toMicrons(pendingWlMin);
                    rawBands[currentRawIdx].wlMax    = toMicrons(pendingWlMax);
                    rawBands[currentRawIdx].wlCentral = toMicrons(pendingWlCentral);
                }
            }
            break;
        }
        default:
            break;
        }
    }

    // ── 组装 SensorBandInfo ──
    // 物理波段名 → bandNumber 映射 (用于辐射定标算法的波段号)
    static const QMap<QString, int> physToNum = {
        {"B1", 1},  {"B2", 2},  {"B3", 3},  {"B4", 4},
        {"B5", 5},  {"B6", 6},  {"B7", 7},  {"B8", 8},
        {"B8A",9},  {"B9", 10}, {"B10",11}, {"B11",12}, {"B12",13}
    };
    static const QMap<QString, QString> physToName = {
        {"B1", "Coastal/Aerosol"}, {"B2", "Blue"}, {"B3", "Green"},
        {"B4", "Red"}, {"B5", "Red-Edge-1"}, {"B6", "Red-Edge-2"},
        {"B7", "Red-Edge-3"}, {"B8", "NIR-Broad"}, {"B8A", "NIR-Narrow"},
        {"B9", "Water Vapour"}, {"B10", "SWIR-Cirrus"},
        {"B11", "SWIR-1"}, {"B12", "SWIR-2"}
    };

    for (const RawBand& rb : rawBands)
    {
        SensorBandInfo b;
        b.physicalBand     = rb.physicalBand;
        b.bandNumber       = physToNum.value(rb.physicalBand, rb.bandId + 1);
        b.bandName         = physToName.value(rb.physicalBand, rb.physicalBand);
        b.resolution       = rb.resolution;
        b.wavelengthMin    = rb.wlMin;
        b.wavelengthMax    = rb.wlMax;
        b.wavelengthCentral = rb.wlCentral;
        b.solarIrradiance  = irradiances.value(rb.bandId, 0.0);
        b.radioAddOffset   = radioOffsets.value(rb.bandId, -1000.0);
        b.physicalGain     = physicalGains.value(rb.bandId, 1.0);
        // L1C: gain = 1/Q, offset = 0 (基础 DN→反射率)
        b.gain             = 1.0 / info.quantificationValue;
        b.offset           = 0.0;

        info.bands.append(b);
    }

    // ── L2A 产品：尝试提取大气参数（AOT / 水汽）──
    // L2A XML 结构不同于 L1C，使用正则搜索原始文本中最常见的关键标签
    {
        static const QRegularExpression aotRx(
            QStringLiteral(R"(<(?:Mean_)?AOT[^>]*>([0-9.]+)</(?:Mean_)?AOT>)"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch aotMatch = aotRx.match(QString::fromUtf8(xmlData));
        if (aotMatch.hasMatch())
            info.meanAOT = aotMatch.captured(1).toDouble();

        static const QRegularExpression wvRx(
            QStringLiteral(R"(<(?:Mean_)?WV[^>]*>([0-9.]+)</(?:Mean_)?WV>)"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch wvMatch = wvRx.match(QString::fromUtf8(xmlData));
        if (wvMatch.hasMatch())
            info.meanWV = wvMatch.captured(1).toDouble();
    }

    qDebug() << "[Sentinel2Product] parsed MTD_MSIL1C.xml:"
             << info.bands.size() << "bands,"
             << "quantValue:" << info.quantificationValue
             << "U:" << info.reflectanceU
             << "sunZenith:" << info.solarZenithAngle
             << "sunAzimuth:" << info.solarAzimuthAngle
             << "viewZenith:" << info.sensorZenithAngle
             << "viewAzimuth:" << info.sensorAzimuthAngle
             << "earthSunDist:" << info.earthSunDistance
             << "meanAOT:" << info.meanAOT
             << "meanWV:" << info.meanWV;

    return info;
}

// ─── 解析 Tile 级 MTD_TL.xml 中的太阳角度 ───────────────────────────────────
// Baseline >= 05.00 将 Mean_Sun_Angle 从产品级 MTD_MSIL1C.xml 移到了此处

void Sentinel2Product::parseMtdTlSunAngles(const QString& tlXmlPath, SensorInfo& info) const
{
    QByteArray xmlData = readTextFile(tlXmlPath);
    if (xmlData.isEmpty())
    {
        qDebug() << "[Sentinel2Product] MTD_TL.xml not found at" << tlXmlPath;
        return;
    }

    QXmlStreamReader xml(xmlData);
    bool inMeanSun = false;
    bool inMeanViewing = false;
    QString pendingElement;
    QString pendingViewingElement;

    while (!xml.atEnd() && !xml.hasError())
    {
        switch (xml.readNext())
        {
        case QXmlStreamReader::StartElement: {
            QString name = xml.name().toString();
            if (name == QStringLiteral("Mean_Sun_Angle"))
                inMeanSun = true;
            else if (name == QStringLiteral("Mean_Viewing_Incidence_Angle"))
                inMeanViewing = true;
            else if (inMeanSun && (name == QStringLiteral("ZENITH_ANGLE")
                                   || name == QStringLiteral("AZIMUTH_ANGLE")))
                pendingElement = name;
            else if (inMeanViewing && (name == QStringLiteral("ZENITH_ANGLE")
                                       || name == QStringLiteral("AZIMUTH_ANGLE")))
                pendingViewingElement = name;
            break;
        }
        case QXmlStreamReader::EndElement: {
            QString endName = xml.name().toString();
            if (endName == QStringLiteral("Mean_Sun_Angle"))
                inMeanSun = false;
            else if (endName == QStringLiteral("Mean_Viewing_Incidence_Angle"))
                inMeanViewing = false;
            break;
        }
        case QXmlStreamReader::Characters: {
            QString text = xml.text().toString().trimmed();
            if (text.isEmpty())
                break;
            if (!pendingElement.isEmpty())
            {
                if (pendingElement == QStringLiteral("ZENITH_ANGLE"))
                    info.solarZenithAngle = text.toDouble();
                else if (pendingElement == QStringLiteral("AZIMUTH_ANGLE"))
                    info.solarAzimuthAngle = text.toDouble();
                pendingElement.clear();
            }
            else if (!pendingViewingElement.isEmpty())
            {
                if (pendingViewingElement == QStringLiteral("ZENITH_ANGLE"))
                    info.sensorZenithAngle = text.toDouble();
                else if (pendingViewingElement == QStringLiteral("AZIMUTH_ANGLE"))
                    info.sensorAzimuthAngle = text.toDouble();
                pendingViewingElement.clear();
            }
            break;
        }
        default:
            break;
        }
    }

    qDebug() << "[Sentinel2Product] MTD_TL angles: sunZenith="
             << info.solarZenithAngle << "sunAzimuth=" << info.solarAzimuthAngle
             << "viewZenith=" << info.sensorZenithAngle
             << "viewAzimuth=" << info.sensorAzimuthAngle;
}

// ─── public accessors ───────────────────────────────────────────────────────

QList<RasterBandDescriptor> Sentinel2Product::bands() const
{
    return mBands;
}

QList<RasterBandDescriptor> Sentinel2Product::bandsAtResolution(double res) const
{
    QList<RasterBandDescriptor> result;
    for (const auto& b : mBands)
    {
        if (qAbs(b.resolution - res) < 0.5)
            result.append(b);
    }
    return result;
}

SensorInfo Sentinel2Product::sensorInfo() const
{
    return mSensorInfo;
}

QString Sentinel2Product::sensorType() const
{
    return mPlatform;
}

QString Sentinel2Product::productId() const
{
    return mSafeDirName;
}

QString Sentinel2Product::previewImagePath() const
{
    if (mSafeDirName.isEmpty())
        return {};
    // ql.jpg 快视图在 SAFE 根目录下
    return mRootPath + QStringLiteral("/") + mSafeDirName + QStringLiteral("-ql.jpg");
}

QString Sentinel2Product::originalPath() const
{
    return mOriginalPath;
}
