#ifndef PRODUCTDESCRIPTOR_H
#define PRODUCTDESCRIPTOR_H

#include <QString>
#include <QList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QFile>

/// 通用遥感处理产物描述符 — 替代传感器专属描述文件（如 s2desc）
/// 一种格式覆盖所有传感器、所有处理阶段，自带处理溯源
struct ProductDescriptor
{
    // ── 基本元数据 ──
    QString formatVersion = QStringLiteral("1.0");
    QString productId;
    QString generatedAt;
    QString sensorType;
    QString originalPath;   // 原始产品路径（相对描述文件）

    // ── 处理溯源 ──
    struct ProcessingStep
    {
        QString step;       // "radiometric", "geometric", "fusion", "mosaic"
        QString algorithm;  // "6S", "Py6S", "Sen2Cor", "RPC", "IHS" ...
        QJsonObject params;
        QString timestamp;
    };
    QList<ProcessingStep> processingHistory;

    // ── 波段条目 ──
    struct BandEntry
    {
        int     physicalBand = -1;
        QString bandName;
        QString rasterPath;       // 相对描述文件路径
        double  resolution  = 0.0;
        int     rasterWidth  = 0;
        int     rasterHeight = 0;
        QString dataType;
        double  wavelengthMin     = 0.0;
        double  wavelengthMax     = 0.0;
        double  wavelengthCentral = 0.0;
        double  gain          = 1.0;
        double  offset        = 0.0;
        double  solarIrradiance = 0.0;
        double  noDataValue   = 0.0;
    };
    QList<BandEntry> bands;

    // ── 空间范围 ──
    struct Extent
    {
        QString crs;
        double  xMin = 0, yMin = 0, xMax = 0, yMax = 0;
    };
    Extent extent;

    // ── 传感器元数据（可序列化子集）──
    struct SensorInfoData
    {
        QString sensorId;
        QString acquisitionTime;
        double  solarZenithAngle   = 0.0;
        double  solarAzimuthAngle  = 0.0;
        double  sensorZenithAngle  = 0.0;
        double  sensorAzimuthAngle = 0.0;
        double  earthSunDistance   = 1.0;
        double  quantificationValue = 10000.0;
        double  reflectanceU       = 1.0;
        int     nodataValue        = 0;
        int     saturatedValue     = 65535;
    };
    SensorInfoData sensorInfoData;

    // ── 序列化 ──
    bool save(const QString& outputPath) const
    {
        QJsonObject root;
        root["formatVersion"] = formatVersion;
        root["productId"]     = productId;
        root["generatedAt"]   = generatedAt;
        root["sensorType"]    = sensorType;
        root["originalPath"]  = originalPath;

        // processingHistory
        QJsonArray histArr;
        for (const auto& s : processingHistory)
        {
            QJsonObject o;
            o["step"] = s.step; o["algorithm"] = s.algorithm;
            o["params"] = s.params; o["timestamp"] = s.timestamp;
            histArr.append(o);
        }
        root["processingHistory"] = histArr;

        // bands
        QJsonArray bandArr;
        for (const auto& b : bands)
        {
            QJsonObject o;
            o["physicalBand"] = b.physicalBand;
            o["bandName"]     = b.bandName;
            o["rasterPath"]   = b.rasterPath;
            o["resolution"]   = b.resolution;
            o["rasterWidth"]  = b.rasterWidth;
            o["rasterHeight"] = b.rasterHeight;
            o["dataType"]     = b.dataType;
            o["wavelengthMin"]     = b.wavelengthMin;
            o["wavelengthMax"]     = b.wavelengthMax;
            o["wavelengthCentral"] = b.wavelengthCentral;
            o["gain"]          = b.gain;
            o["offset"]        = b.offset;
            o["solarIrradiance"] = b.solarIrradiance;
            o["noDataValue"]   = b.noDataValue;
            bandArr.append(o);
        }
        root["bands"] = bandArr;

        // extent
        if (!extent.crs.isEmpty())
        {
            QJsonObject e;
            e["crs"] = extent.crs;
            e["xMin"] = extent.xMin; e["yMin"] = extent.yMin;
            e["xMax"] = extent.xMax; e["yMax"] = extent.yMax;
            root["extent"] = e;
        }

        // sensorInfo
        {
            QJsonObject si;
            si["sensorId"]             = sensorInfoData.sensorId;
            si["acquisitionTime"]      = sensorInfoData.acquisitionTime;
            si["solarZenithAngle"]     = sensorInfoData.solarZenithAngle;
            si["solarAzimuthAngle"]    = sensorInfoData.solarAzimuthAngle;
            si["sensorZenithAngle"]    = sensorInfoData.sensorZenithAngle;
            si["sensorAzimuthAngle"]   = sensorInfoData.sensorAzimuthAngle;
            si["earthSunDistance"]     = sensorInfoData.earthSunDistance;
            si["quantificationValue"]  = sensorInfoData.quantificationValue;
            si["reflectanceU"]         = sensorInfoData.reflectanceU;
            si["nodataValue"]          = sensorInfoData.nodataValue;
            si["saturatedValue"]       = sensorInfoData.saturatedValue;
            root["sensorInfo"] = si;
        }

        QJsonDocument doc(root);
        QFile f(outputPath);
        return f.open(QIODevice::WriteOnly)
               && f.write(doc.toJson(QJsonDocument::Indented)) > 0;
    }

    static ProductDescriptor load(const QString& path)
    {
        ProductDescriptor d;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return d;

        QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        d.formatVersion = root.value("formatVersion").toString();
        d.productId     = root.value("productId").toString();
        d.generatedAt   = root.value("generatedAt").toString();
        d.sensorType    = root.value("sensorType").toString();
        d.originalPath  = root.value("originalPath").toString();

        // processingHistory
        for (const QJsonValue& v : root.value("processingHistory").toArray())
        {
            QJsonObject o = v.toObject();
            ProcessingStep s;
            s.step      = o.value("step").toString();
            s.algorithm = o.value("algorithm").toString();
            s.params    = o.value("params").toObject();
            s.timestamp = o.value("timestamp").toString();
            d.processingHistory.append(s);
        }

        // bands
        for (const QJsonValue& v : root.value("bands").toArray())
        {
            QJsonObject o = v.toObject();
            BandEntry b;
            b.physicalBand   = o.value("physicalBand").toInt(-1);
            b.bandName       = o.value("bandName").toString();
            b.rasterPath     = o.value("rasterPath").toString();
            b.resolution     = o.value("resolution").toDouble();
            b.rasterWidth    = o.value("rasterWidth").toInt();
            b.rasterHeight   = o.value("rasterHeight").toInt();
            b.dataType       = o.value("dataType").toString();
            b.wavelengthMin     = o.value("wavelengthMin").toDouble();
            b.wavelengthMax     = o.value("wavelengthMax").toDouble();
            b.wavelengthCentral = o.value("wavelengthCentral").toDouble();
            b.gain           = o.value("gain").toDouble(1.0);
            b.offset         = o.value("offset").toDouble();
            b.solarIrradiance = o.value("solarIrradiance").toDouble();
            b.noDataValue    = o.value("noDataValue").toDouble();
            d.bands.append(b);
        }

        // extent
        QJsonObject e = root.value("extent").toObject();
        if (!e.isEmpty())
        {
            d.extent.crs  = e.value("crs").toString();
            d.extent.xMin = e.value("xMin").toDouble();
            d.extent.yMin = e.value("yMin").toDouble();
            d.extent.xMax = e.value("xMax").toDouble();
            d.extent.yMax = e.value("yMax").toDouble();
        }

        // sensorInfo
        QJsonObject si = root.value("sensorInfo").toObject();
        if (!si.isEmpty())
        {
            d.sensorInfoData.sensorId             = si.value("sensorId").toString();
            d.sensorInfoData.acquisitionTime      = si.value("acquisitionTime").toString();
            d.sensorInfoData.solarZenithAngle     = si.value("solarZenithAngle").toDouble();
            d.sensorInfoData.solarAzimuthAngle    = si.value("solarAzimuthAngle").toDouble();
            d.sensorInfoData.sensorZenithAngle    = si.value("sensorZenithAngle").toDouble();
            d.sensorInfoData.sensorAzimuthAngle   = si.value("sensorAzimuthAngle").toDouble();
            d.sensorInfoData.earthSunDistance     = si.value("earthSunDistance").toDouble(1.0);
            d.sensorInfoData.quantificationValue  = si.value("quantificationValue").toDouble(10000.0);
            d.sensorInfoData.reflectanceU         = si.value("reflectanceU").toDouble(1.0);
            d.sensorInfoData.nodataValue          = si.value("nodataValue").toInt();
            d.sensorInfoData.saturatedValue       = si.value("saturatedValue").toInt(65535);
        }

        return d;
    }
};

#endif // PRODUCTDESCRIPTOR_H
