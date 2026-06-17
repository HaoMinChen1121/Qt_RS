#include "GfProduct.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>

bool GfProduct::open(const QString& path)
{
    close();

    QFileInfo fi(path);
    if (!fi.exists())
    {
        qWarning() << "[GfProduct] path does not exist:" << path;
        return false;
    }

    mOriginalPath = fi.absoluteFilePath();
    QDir workDir  = fi.isDir() ? QDir(mOriginalPath) : fi.dir();

    // 扫描 TIFF 文件
    QStringList tifFiles = workDir.entryList({"*.tif", "*.tiff", "*.TIF", "*.TIFF"}, QDir::Files);
    tifFiles.sort();

    // 查找 XML 元数据文件
    QString xmlPath;
    QStringList xmlFiles = workDir.entryList({"*.xml", "*.XML"}, QDir::Files);
    if (!xmlFiles.isEmpty())
        xmlPath = workDir.absoluteFilePath(xmlFiles.first());

    // 自动识别传感器类型
    mSensorType = guessSensorType(workDir.dirName(), tifFiles);

    // 解析元数据
    if (!xmlPath.isEmpty())
        mSensorInfo = parseGfXml(xmlPath);
    else
        mSensorInfo = SensorInfo();

    mSensorInfo.sensorType = mSensorType;
    mSensorInfo.sensorId   = mSensorType;

    // GF 波段定义
    struct GfBandDef { int num; const char* name; double wlMin; double wlMax; double res; };
    static const GfBandDef gfCommon[] = {
        {1, "Blue",  0.45, 0.52, 8.0},
        {2, "Green", 0.52, 0.59, 8.0},
        {3, "Red",   0.63, 0.69, 8.0},
        {4, "NIR",   0.77, 0.89, 8.0},
    };
    static const GfBandDef gfPan = {5, "Pan", 0.45, 0.90, 2.0};

    // 根据文件数量推断波段配置
    int bandCount = tifFiles.size();

    for (const auto& bd : gfCommon)
    {
        // 匹配波段文件 (常见命名: xxx_B1.tif, xxx-band1.tif)
        RasterBandDescriptor desc;
        desc.physicalBand = bd.num;
        desc.bandName     = QString::fromLatin1(bd.name);
        desc.resolution   = bd.res;
        desc.dataType     = QStringLiteral("UInt16");

        // 按序号取文件 (简单策略：排序后的第 N 个文件对应第 N 波段)
        if (bd.num - 1 < tifFiles.size())
            desc.rasterPath = workDir.absoluteFilePath(tifFiles[bd.num - 1]);
        else
            continue;

        mBands.append(desc);
    }

    // Pan 波段 (GF-1/2: 文件数5=含Pan, GF-6: 文件数9=含Pan)
    if (bandCount >= 5)
    {
        RasterBandDescriptor desc;
        desc.physicalBand = gfPan.num;
        desc.bandName     = gfPan.name;
        desc.resolution   = gfPan.res;
        desc.dataType     = QStringLiteral("UInt16");
        if (bandCount > 4)
            desc.rasterPath = workDir.absoluteFilePath(tifFiles[4]);
        mBands.append(desc);
    }

    std::sort(mBands.begin(), mBands.end(),
              [](const RasterBandDescriptor& a, const RasterBandDescriptor& b)
              {
                  return a.physicalBand < b.physicalBand;
              });

    mProductId = workDir.dirName();
    mOpen = true;
    qDebug() << "[GfProduct] opened:" << mProductId << "sensor:" << mSensorType << "bands:" << mBands.size();
    return true;
}

void GfProduct::close()
{
    mBands.clear();
    mSensorInfo = SensorInfo();
    mSensorType.clear();
    mProductId.clear();
    mOpen = false;
}

bool GfProduct::isOpen() const { return mOpen; }
QList<RasterBandDescriptor> GfProduct::bands() const { return mBands; }

QList<RasterBandDescriptor> GfProduct::bandsAtResolution(double res) const
{
    QList<RasterBandDescriptor> result;
    for (const auto& b : mBands)
    {
        if (qAbs(b.resolution - res) < 1.0)
            result.append(b);
    }
    return result;
}

SensorInfo GfProduct::sensorInfo() const { return mSensorInfo; }
QString GfProduct::sensorType()   const { return mSensorType; }
QString GfProduct::productId()    const { return mProductId; }
QString GfProduct::previewImagePath() const { return {}; }
QString GfProduct::originalPath() const { return mOriginalPath; }

// ─── 传感器类型自动识别 ─────────────────────────────────────────────────────

QString GfProduct::guessSensorType(const QString& dirName, const QStringList& tifFiles) const
{
    int nFiles = tifFiles.size();

    // GF-1 WFV: 4 波段 (多光谱), +1 Pan (2m) = 5
    // GF-2 PMS: 4 MS + 1 Pan = 5
    // GF-6 WFV: 8 MS + 1 Pan = 9

    if (dirName.contains("GF-1", Qt::CaseInsensitive) || dirName.contains("GF1", Qt::CaseInsensitive))
        return QStringLiteral("GF-1");
    if (dirName.contains("GF-2", Qt::CaseInsensitive) || dirName.contains("GF2", Qt::CaseInsensitive))
        return QStringLiteral("GF-2");
    if (dirName.contains("GF-6", Qt::CaseInsensitive) || dirName.contains("GF6", Qt::CaseInsensitive))
        return QStringLiteral("GF-6");

    // 根据文件数推测
    if (nFiles >= 9)  return QStringLiteral("GF-6");
    if (nFiles >= 5)  return QStringLiteral("GF-1");
    if (nFiles == 4)  return QStringLiteral("GF-1");

    return QStringLiteral("GF-1");
}

// ─── XML metadata parser ────────────────────────────────────────────────────

SensorInfo GfProduct::parseGfXml(const QString& xmlPath) const
{
    SensorInfo info;

    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return info;

    QXmlStreamReader xml(&file);
    QMap<QString, QString> leaf;
    QStringList stack;
    while (!xml.atEnd() && !xml.hasError())
    {
        switch (xml.readNext())
        {
        case QXmlStreamReader::StartElement:
            stack.append(xml.name().toString());
            break;
        case QXmlStreamReader::EndElement:
            if (!stack.isEmpty()) stack.removeLast();
            break;
        case QXmlStreamReader::Characters: {
            QString text = xml.text().toString().trimmed();
            if (!text.isEmpty() && !stack.isEmpty())
                leaf.insert(stack.join('/'), text);
            break;
        }
        default: break;
        }
    }
    file.close();

    auto find = [&](const QStringList& keys) -> QString {
        for (auto it = leaf.begin(); it != leaf.end(); ++it)
            for (const QString& k : keys)
                if (it.key().contains(k, Qt::CaseInsensitive))
                    return it.value();
        return {};
    };

    QString acqStr = find({"acquisitionTime", "AcquisitionTime", "ImagingTime", "CenterTime"});
    if (!acqStr.isEmpty())
        info.acquisitionTime = QDateTime::fromString(acqStr, Qt::ISODate);

    info.solarZenithAngle  = find({"SolarZenith", "solarZenith", "SunElevation"}).toDouble();
    info.solarAzimuthAngle = find({"SolarAzimuth", "solarAzimuth", "SunAzimuth"}).toDouble();

    return info;
}
