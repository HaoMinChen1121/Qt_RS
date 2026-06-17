#ifndef SENTINEL2PRODUCT_H
#define SENTINEL2PRODUCT_H

#include "dataaccess/ISensorProduct.h"
#include <QMap>

class Sentinel2Product : public ISensorProduct
{
public:
    bool open(const QString& path) override;
    void close() override;
    bool isOpen() const override;

    QList<RasterBandDescriptor> bands() const override;
    QList<RasterBandDescriptor> bandsAtResolution(double res) const override;

    SensorInfo sensorInfo() const override;
    QString sensorType() const override;
    QString productId() const override;

    QString previewImagePath() const override;
    QString originalPath() const override;

private:
    bool openFromZip(const QString& zipPath);
    bool openFromSafeDir(const QString& safeDirPath);

    bool readManifestXml(const QString& manifestPath);
    SensorInfo parseMtdMsil1cXml(const QString& xmlPath) const;
    void parseMtdTlSunAngles(const QString& tlXmlPath, SensorInfo& info) const;

    /// 通过 GDAL VSIF 读取整个文本文件 (支持 /vsizip/)
    static QByteArray readTextFile(const QString& vsiPath);
    /// 通过 GDAL 打开栅格文件获取波段尺寸信息
    static QSize probeRasterSize(const QString& rasterPath);

    QString            mOriginalPath;
    QString            mRootPath;        // SAFE 根路径 (可能是 /vsizip/...)
    QString            mSafeDirName;
    QString            mPlatform;        // "Sentinel-2A" or "Sentinel-2B"
    QString            mTileId;
    QString            mGranuleDirName;
    QList<RasterBandDescriptor> mBands;
    SensorInfo         mSensorInfo;
    bool               mOpen = false;
};

#endif // SENTINEL2PRODUCT_H
