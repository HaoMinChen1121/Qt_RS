#include "LandsatProduct.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <algorithm>
#include <cpl_vsi.h>
#include <gdal_priv.h>

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

    // ── .tar 归档 ──
    if (path.endsWith(".tar", Qt::CaseInsensitive))
        return openFromTar(mOriginalPath);

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
    mRootPath.clear();
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

// ─── Tar archive open ───────────────────────────────────────────────────────

QByteArray LandsatProduct::readTextFile(const QString& vsiPath)
{
    VSILFILE* fp = VSIFOpenExL(vsiPath.toUtf8().constData(), "rb", FALSE);
    if (!fp)
    {
        qWarning() << "[LandsatProduct] VSIFOpenExL failed:" << vsiPath;
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

bool LandsatProduct::openFromTar(const QString& tarPath)
{
    GDALAllRegister();
    mRootPath = QStringLiteral("/vsitar/") + tarPath;

    // 用 tar -tf 发现文件列表
    QStringList tarFiles;
    QProcess proc;
    proc.start(QStringLiteral("tar"), {QStringLiteral("-tf"), tarPath});
    if (proc.waitForFinished(15000) && proc.exitCode() == 0)
    {
        QString output = QString::fromUtf8(proc.readAllStandardOutput());
        tarFiles = output.split('\n', Qt::SkipEmptyParts);
    }

    // 如果 tar 命令不可用或失败，回退到基于产品 ID 的命名推断
    QFileInfo fi(tarPath);
    QString productId = fi.completeBaseName();

    // 查找 MTL 文件: 优先从 tar 列表中找，否则尝试产品名推断
    QString mtlVsiPath;
    QString mtlFileName;
    if (!tarFiles.isEmpty())
    {
        for (const QString& f : tarFiles)
        {
            QString trim = f.trimmed();
            if (trim.endsWith("_MTL.txt", Qt::CaseInsensitive))
            {
                mtlFileName = trim;
                mtlVsiPath = mRootPath + QStringLiteral("/") + trim;
                break;
            }
        }
    }
    if (mtlVsiPath.isEmpty())
    {
        mtlFileName = productId + QStringLiteral("_MTL.txt");
        mtlVsiPath = mRootPath + QStringLiteral("/") + mtlFileName;
    }

    // 读取并解析 MTL
    QByteArray mtlData = readTextFile(mtlVsiPath);
    if (mtlData.isEmpty())
    {
        qWarning() << "[LandsatProduct] failed to read MTL from tar:" << mtlVsiPath;
        return false;
    }

    mSensorInfo = parseMtlData(mtlData);
    mSensorInfo.sensorType = mSensorType;
    mSensorInfo.sensorId   = mSensorType;

    // 构建波段描述符 — 使用 /vsitar/ 虚拟路径
    auto addBand = [&](int num, const char* name, double res, double wlMin, double wlMax)
    {
        // 在 tar 列表中查找该波段文件
        QString bandPath;
        QString zeroPad = QStringLiteral("_B%1.").arg(num, 2, 10, QChar('0'));
        QString noPad   = QStringLiteral("_B%1.").arg(num);

        if (!tarFiles.isEmpty())
        {
            for (const QString& f : tarFiles)
            {
                if (f.contains(zeroPad, Qt::CaseInsensitive) || f.contains(noPad, Qt::CaseInsensitive))
                {
                    bandPath = mRootPath + QStringLiteral("/") + f.trimmed();
                    break;
                }
            }
        }
        if (bandPath.isEmpty())
        {
            QString guess = mRootPath + QStringLiteral("/") + productId + zeroPad + QStringLiteral("TIF");
            bandPath = guess;
        }

        RasterBandDescriptor desc;
        desc.physicalBand = num;
        desc.bandName     = QString::fromLatin1(name);
        desc.rasterPath   = bandPath;
        desc.resolution   = res;
        desc.dataType     = QStringLiteral("UInt16");
        mBands.append(desc);
    };

    struct BandDef { int num; const char* name; double wlMin; double wlMax; double res; };
    static const BandDef defs[] = {
        {1,  "Coastal/Aerosol", 0.433, 0.453, 30.0},
        {2,  "Blue",            0.450, 0.515, 30.0},
        {3,  "Green",           0.525, 0.600, 30.0},
        {4,  "Red",             0.630, 0.680, 30.0},
        {5,  "NIR",             0.845, 0.885, 30.0},
        {6,  "SWIR-1",          1.560, 1.660, 30.0},
        {7,  "SWIR-2",          2.100, 2.300, 30.0},
        {8,  "Pan",             0.500, 0.680, 15.0},
        {9,  "Cirrus",          1.360, 1.390, 30.0},
        {10, "TIRS-1",          10.60, 11.19, 100.0},
        {11, "TIRS-2",          11.50, 12.51, 100.0},
    };

    for (const auto& bd : defs)
        addBand(bd.num, bd.name, bd.res, bd.wlMin, bd.wlMax);

    std::sort(mBands.begin(), mBands.end(),
              [](const RasterBandDescriptor& a, const RasterBandDescriptor& b)
              {
                  return a.physicalBand < b.physicalBand;
              });

    mProductId = productId;
    mOpen = true;
    qDebug() << "[LandsatProduct] opened TAR product:" << mProductId
             << "bands:" << mBands.size() << "sensor:" << mSensorType;
    return true;
}

// ─── MTL parser ─────────────────────────────────────────────────────────────

SensorInfo LandsatProduct::parseMtlData(const QByteArray& data)
{
    SensorInfo info;

    QMap<QString, QString> kv;
    QRegularExpression kvRx(R"(^\s*([A-Za-z_0-9]+)\s*=\s*(.+?)\s*$)");
    const auto lines = data.split('\n');
    for (const QByteArray& raw : lines)
    {
        QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty() || line.startsWith("GROUP") || line.startsWith("END_GROUP"))
            continue;
        QRegularExpressionMatch m = kvRx.match(line);
        if (m.hasMatch())
            kv.insert(m.captured(1), m.captured(2).remove('"'));
    }

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

SensorInfo LandsatProduct::parseMtl(const QString& mtlPath)
{
    QFile file(mtlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QByteArray data = file.readAll();
    file.close();
    return parseMtlData(data);
}
