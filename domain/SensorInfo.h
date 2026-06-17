#ifndef SENSORINFO_H
#define SENSORINFO_H

#include <QString>
#include <QVector>
#include <QDateTime>

struct SensorBandInfo
{
    int     bandNumber       = 1;       // 物理波段编号 (1=B1, 8=B8, 9=B8A)
    QString bandName;                   // "Blue", "NIR", "SWIR-1" ...
    QString physicalBand;              // "B1", "B8A" — 与 XML 一致的标识
    double  wavelengthMin    = 0.0;    // micrometers
    double  wavelengthMax    = 0.0;
    double  wavelengthCentral = 0.0;   // micrometers
    double  resolution       = 0.0;    // 空间分辨率 (m)
    double  gain             = 1.0;
    double  offset           = 0.0;
    double  solarIrradiance  = 0.0;    // W/m2/um
    double  radioAddOffset   = 0.0;    // RADIO_ADD_OFFSET (S2 L1C)
    double  physicalGain     = 1.0;    // PHYSICAL_GAINS (S2 L1C)
};

struct SensorInfo
{
    QString   sensorType;
    QString   sensorId;
    QString   tileId;
    QDateTime acquisitionTime;
    double    solarZenithAngle   = 0.0;
    double    solarAzimuthAngle  = 0.0;
    double    sensorZenithAngle  = 0.0;
    double    sensorAzimuthAngle = 0.0;
    double    earthSunDistance   = 1.0;  // AU

    // ── Sentinel-2 L1C 特定参数 ──
    double    quantificationValue = 10000.0;
    double    reflectanceU        = 1.0;  // U 因子 (TOA 反射率转换)
    int       nodataValue         = 0;
    int       saturatedValue      = 65535;

    // ── L2A 大气参数（从 Sen2Cor 产品元数据中检索，无则保留 0）──
    double    meanAOT             = 0.0;  // Mean AOT at 550nm (from L2A AOT_Retrieval)
    double    meanWV              = 0.0;  // Mean Water Vapour g/cm² (from L2A WV_Retrieval)

    QVector<SensorBandInfo> bands;

    /// 按 bandNumber 查找增益/偏置
    bool bandCalibration(int bandNumber, double& gain, double& offset) const
    {
        for (const auto& b : bands)
        {
            if (b.bandNumber == bandNumber)
            {
                gain   = b.gain;
                offset = b.offset;
                return true;
            }
        }
        return false;
    }

    /// 按 physicalBand 字符串查找波段信息
    const SensorBandInfo* findBand(const QString& physicalBand) const
    {
        for (const auto& b : bands)
        {
            if (b.physicalBand == physicalBand)
                return &b;
        }
        return nullptr;
    }
};

#endif // SENSORINFO_H
