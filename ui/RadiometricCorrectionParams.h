#ifndef RADIOMETRICCORRECTIONPARAMS_H
#define RADIOMETRICCORRECTIONPARAMS_H

#include <QString>
#include <QStringList>

/**
 * @brief 辐射定标与大气校正参数结构体（纯数据载体，不包含业务逻辑）
 */
struct RadiometricCorrectionParams
{
    // 传感器与输入
    QString sensorType;              // Landsat-8, Sentinel-2A, GF-2, etc.
    QStringList inputFiles;          // 输入影像文件列表
    QString metadataFile;            // 元数据文件路径
    QString outputDirectory;         // 输出目录

    // 标定参数
    QString calibrationType;         // "DN2Radiance" / "DN2Reflectance"
    bool autoGainOffset = true;      // 是否从元数据自动读取增益/偏置
    double manualGain = 1.0;
    double manualOffset = 0.0;
    double solarZenithAngle = 0.0;
    double earthSunDistance = 1.0;
    QString outputDataType;          // "Float32" / "UInt16" / "Int16"

    // 大气校正参数
    QString atmModel;                // "6S" / "Py6S" / "Sen2Cor" / "None"
    QString aerosolModel;            // "Continental" / "Maritime" / "Urban" / "Desert"
    QString atmosphericModel;        // "Tropical" / "MidLatSummer" / "MidLatWinter" / "SubArcticSummer" / "SubArcticWinter"
    double aot550 = 0.2;            // Aerosol Optical Thickness at 550nm
    double waterVapor = 2.0;        // 水汽含量 (g/cm^2)
    double ozone = 0.3;             // 臭氧含量 (cm-atm)
    double targetElevation = 0.0;   // 目标高程 (km)
    double sensorAltitude = 800.0;  // 传感器高度 (km, 卫星)

    // Sen2Cor 特定参数
    int sen2corResolution = 20;     // 处理分辨率 (m), 10/20/60

    // 输出设置
    QString outputFormat;            // "ENVI" / "GeoTIFF"
    double scaleFactor = 1.0;
    QString namingPattern;           // 输出文件命名模板

    // 批量处理
    bool batchMode = false;
};

#endif // RADIOMETRICCORRECTIONPARAMS_H
