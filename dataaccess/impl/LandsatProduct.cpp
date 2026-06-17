#include "LandsatProduct.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>
#include <algorithm>

bool LandsatProduct::open(const QString& path)
{
    close();

    QFileInfo fi(path);
    if (!fi.exists())
    {
        qWarning() << "[LandsatProduct] path does not exist:" << path;
        return false;
    }

    mOriginalPath = fi.absoluteFilePath();

    // 查找 MTL 文件和工作目录
    QString mtlPath;
    QDir workDir;

    if (fi.isFile() && fi.fileName().endsWith("_MTL.txt", Qt::CaseInsensitive))
    {
        mtlPath  = mOriginalPath;
        workDir  = fi.dir();
    } else if (fi.isDir())
    {
        workDir = QDir(mOriginalPath);
        // 扫描 _MTL.txt
        QStringList mtlFiles = workDir.entryList({"*_MTL.txt"}, QDir::Files);
        if (!mtlFiles.isEmpty())
            mtlPath = workDir.absoluteFilePath(mtlFiles.first());
    }

    if (mtlPath.isEmpty())
    {
        qWarning() << "[LandsatProduct] no _MTL.txt found in:" << mOriginalPath;
        return false;
    }

    // 扫描 B*.TIF 波段文件
    QStringList tifFiles = workDir.entryList({"*_B*.TIF", "*_B*.tif"}, QDir::Files);
    tifFiles.sort();

    // 解析元数据
    mSensorInfo = parseMtl(mtlPath);

    // 查找 TIR (热红外) 波段: *_B10.TIF, *_B11.TIF
    QStringList tirFiles = workDir.entryList({QStringLiteral("*_B1[0-1].TIF"), QStringLiteral("*_B1[0-1].tif")}, QDir::Files);

    // Landsat 波段名映射
    struct LsBandDef { int num; const char* name; };
    static const LsBandDef lsBands[] = {
        {1,  "Coastal/Aerosol"}, {2,  "Blue"}, {3,  "Green"},
        {4,  "Red"}, {5,  "NIR"}, {6,  "SWIR-1"}, {7,  "SWIR-2"},
        {8,  "Pan"}, {9,  "Cirrus"}, {10, "TIRS-1"}, {11, "TIRS-2"},
    };

    for (const auto& bd : lsBands)
    {
        // 匹配文件名如 LC08_L1TP_xxx_B4.TIF
        QString pattern = QStringLiteral("*_B%1.*");
        QStringList matches = workDir.entryList({pattern.arg(bd.num)}, QDir::Files);

        // Try zero-padded B04, B10, B11
        if (matches.isEmpty())
            matches = workDir.entryList({QStringLiteral("*_B%1.*").arg(bd.num, 2, 10, QChar('0'))}, QDir::Files);

        if (matches.isEmpty())
            continue;

        RasterBandDescriptor desc;
        desc.physicalBand = bd.num;
        desc.bandName     = QString::fromLatin1(bd.name);
        desc.rasterPath   = workDir.absoluteFilePath(matches.first());
        desc.resolution   = (bd.num == 8) ? 15.0 : (bd.num >= 10 ? 100.0 : 30.0);
        desc.dataType     = QStringLiteral("UInt16");

        mBands.append(desc);
    }

    std::sort(mBands.begin(), mBands.end(),
              [](const RasterBandDescriptor& a, const RasterBandDescriptor& b)
              {
                  return a.physicalBand < b.physicalBand;
              });

    mSensorInfo.sensorType = mSensorType;
    mSensorInfo.sensorId   = mSensorType;
    mProductId = fi.completeBaseName();

    mOpen = true;
    qDebug() << "[LandsatProduct] opened:" << mProductId << "bands:" << mBands.size();
    return true;
}

void LandsatProduct::close()
{
    mBands.clear();
    mSensorInfo = SensorInfo();
    mSensorType.clear();
    mProductId.clear();
    mOpen = false;
}

bool LandsatProduct::isOpen() const { return mOpen; }

QList<RasterBandDescriptor> LandsatProduct::bands() const { return mBands; }

QList<RasterBandDescriptor> LandsatProduct::bandsAtResolution(double res) const
{
    QList<RasterBandDescriptor> result;
    for (const auto& b : mBands)
    {
        if (qAbs(b.resolution - res) < 1.0)
            result.append(b);
    }
    return result;
}

SensorInfo LandsatProduct::sensorInfo() const { return mSensorInfo; }
QString LandsatProduct::sensorType()   const { return mSensorType; }
QString LandsatProduct::productId()    const { return mProductId; }
QString LandsatProduct::previewImagePath() const { return {}; }
QString LandsatProduct::originalPath() const { return mOriginalPath; }

// ─── MTL parser (复用 SensorMetadataProvider 的逻辑) ────────────────────────

SensorInfo LandsatProduct::parseMtl(const QString& mtlPath)
{
    SensorInfo info;

    QFile file(mtlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return info;

    QMap<QString, QString> kv;
    QRegularExpression kvRx(R"(^\s*([A-Za-z_0-9]+)\s*=\s*(.+?)\s*$)");
    while (!file.atEnd())
    {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith("GROUP") || line.startsWith("END_GROUP"))
            continue;
        QRegularExpressionMatch m = kvRx.match(line);
        if (m.hasMatch())
            kv.insert(m.captured(1), m.captured(2).remove('"'));
    }
    file.close();

    mSensorType = "Landsat-";
    QString sensorId = kv.value("SPACECRAFT_ID", kv.value("SENSOR_ID"));
    if (sensorId.contains("8"))  mSensorType += "8";
    else if (sensorId.contains("9"))  mSensorType += "9";
    else  mSensorType += "8";

    info.sensorType = mSensorType;
    info.sensorId   = sensorId;

    QString dateStr = kv.value("DATE_ACQUIRED");
    QString timeStr = kv.value("SCENE_CENTER_TIME");
    if (!dateStr.isEmpty() && !timeStr.isEmpty())
        info.acquisitionTime = QDateTime::fromString(dateStr + "T" + timeStr, Qt::ISODate);

    info.solarZenithAngle  = 90.0 - kv.value("SUN_ELEVATION").toDouble();
    info.solarAzimuthAngle = kv.value("SUN_AZIMUTH").toDouble();
    info.earthSunDistance  = kv.value("EARTH_SUN_DISTANCE").toDouble();

    struct BandDef { int num; const char* name; double wlMin; double wlMax; };
    static const BandDef defs[] = {
        {1, "Coastal/Aerosol", 0.433, 0.453},
        {2, "Blue",            0.450, 0.515},
        {3, "Green",           0.525, 0.600},
        {4, "Red",             0.630, 0.680},
        {5, "NIR",             0.845, 0.885},
        {6, "SWIR-1",          1.560, 1.660},
        {7, "SWIR-2",          2.100, 2.300},
        {8, "Pan",             0.500, 0.680},
        {9, "Cirrus",          1.360, 1.390},
        {10,"TIRS-1",          10.60, 11.19},
        {11,"TIRS-2",          11.50, 12.51},
    };

    for (const auto& bd : defs)
    {
        SensorBandInfo band;
        band.bandNumber    = bd.num;
        band.bandName      = QString::fromLatin1(bd.name);
        band.physicalBand  = QStringLiteral("B%1").arg(bd.num);
        band.wavelengthMin = bd.wlMin;
        band.wavelengthMax = bd.wlMax;
        band.gain  = kv.value(QStringLiteral("REFLECTANCE_MULT_BAND_%1").arg(bd.num), "1.0").toDouble();
        band.offset = kv.value(QStringLiteral("REFLECTANCE_ADD_BAND_%1").arg(bd.num), "0.0").toDouble();
        band.solarIrradiance = kv.value(QStringLiteral("SOLAR_IRRADIANCE_BAND_%1").arg(bd.num), "0.0").toDouble();
        band.resolution = (bd.num == 8) ? 15.0 : (bd.num >= 10 ? 100.0 : 30.0);
        info.bands.append(band);
    }

    return info;
}
