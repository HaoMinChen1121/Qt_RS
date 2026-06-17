#include "DescriptorProduct.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>

// ─── open / close ───────────────────────────────────────────────────────────

bool DescriptorProduct::open(const QString& path)
{
    close();

    mDescriptor = ProductDescriptor::load(path);
    if (mDescriptor.productId.isEmpty())
    {
        qWarning() << "[DescriptorProduct] failed to load descriptor:" << path;
        return false;
    }

    mOriginalPath = QFileInfo(path).absoluteFilePath();
    QDir descDir  = QFileInfo(path).absoluteDir();

    // ── 重建波段列表 ──
    for (const auto& be : mDescriptor.bands)
    {
        RasterBandDescriptor rbd;
        rbd.physicalBand = be.physicalBand;
        rbd.bandName     = be.bandName;
        rbd.resolution   = be.resolution;
        rbd.dataType     = be.dataType;
        rbd.rasterSize   = QSize(be.rasterWidth, be.rasterHeight);

        // 相对路径 → 绝对路径
        QString rp = be.rasterPath;
        if (QFileInfo(rp).isRelative())
            rp = descDir.absoluteFilePath(rp);
        rbd.rasterPath = rp;

        mBands.append(rbd);
    }

    // ── 重建传感器元数据 ──
    const auto& si = mDescriptor.sensorInfoData;
    mSensorInfo.sensorType = mDescriptor.sensorType;
    mSensorInfo.sensorId   = si.sensorId;
    mSensorInfo.acquisitionTime =
        QDateTime::fromString(si.acquisitionTime, Qt::ISODate);
    mSensorInfo.solarZenithAngle   = si.solarZenithAngle;
    mSensorInfo.solarAzimuthAngle  = si.solarAzimuthAngle;
    mSensorInfo.sensorZenithAngle  = si.sensorZenithAngle;
    mSensorInfo.sensorAzimuthAngle = si.sensorAzimuthAngle;
    mSensorInfo.earthSunDistance   = si.earthSunDistance;
    mSensorInfo.quantificationValue = si.quantificationValue;
    mSensorInfo.reflectanceU       = si.reflectanceU;
    mSensorInfo.nodataValue        = si.nodataValue;
    mSensorInfo.saturatedValue     = si.saturatedValue;

    // 波段级别的 SensorBandInfo
    for (const auto& be : mDescriptor.bands)
    {
        SensorBandInfo sbi;
        sbi.bandNumber        = be.physicalBand;
        sbi.bandName          = be.bandName;
        sbi.physicalBand      = QStringLiteral("B%1").arg(be.physicalBand);
        sbi.wavelengthMin     = be.wavelengthMin;
        sbi.wavelengthMax     = be.wavelengthMax;
        sbi.wavelengthCentral = be.wavelengthCentral;
        sbi.gain              = be.gain;
        sbi.offset            = be.offset;
        sbi.solarIrradiance   = be.solarIrradiance;
        sbi.resolution        = be.resolution;
        mSensorInfo.bands.append(sbi);
    }

    mOpen = true;
    qDebug() << "[DescriptorProduct] opened:" << mDescriptor.productId
             << "bands:" << mBands.size()
             << "history:" << mDescriptor.processingHistory.size();
    return true;
}

void DescriptorProduct::close()
{
    mBands.clear();
    mSensorInfo = SensorInfo();
    mDescriptor = ProductDescriptor();
    mOriginalPath.clear();
    mOpen = false;
}

bool DescriptorProduct::isOpen() const { return mOpen; }

// ─── accessors ──────────────────────────────────────────────────────────────

QList<RasterBandDescriptor> DescriptorProduct::bands() const
{
    return mBands;
}

QList<RasterBandDescriptor> DescriptorProduct::bandsAtResolution(double res) const
{
    QList<RasterBandDescriptor> result;
    for (const auto& b : mBands)
        if (qAbs(b.resolution - res) < 0.5)
            result.append(b);
    return result;
}

SensorInfo DescriptorProduct::sensorInfo() const { return mSensorInfo; }
QString DescriptorProduct::sensorType()   const { return mDescriptor.sensorType; }
QString DescriptorProduct::productId()    const { return mDescriptor.productId; }
QString DescriptorProduct::previewImagePath() const { return {}; }
QString DescriptorProduct::originalPath() const { return mOriginalPath; }

// ─── 工厂辅助：从任意 ISensorProduct 构建通用描述符 ─────────────────────────

ProductDescriptor DescriptorProduct::buildDescriptor(ISensorProduct* product,
                                                      const QString& originalPath)
                                                      {
    ProductDescriptor desc;
    if (!product)
        return desc;

    desc.productId   = product->productId();
    desc.sensorType  = product->sensorType();
    desc.generatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    desc.originalPath = originalPath;

    // 波段
    QList<RasterBandDescriptor> bds = product->bands();
    for (const auto& rbd : bds)
    {
        ProductDescriptor::BandEntry be;
        be.physicalBand = rbd.physicalBand;
        be.bandName     = rbd.bandName;
        be.rasterPath   = rbd.rasterPath;
        be.resolution   = rbd.resolution;
        be.rasterWidth  = rbd.rasterSize.width();
        be.rasterHeight = rbd.rasterSize.height();
        be.dataType     = rbd.dataType;
        desc.bands.append(be);
    }

    // 传感器元数据
    SensorInfo info = product->sensorInfo();
    desc.sensorInfoData.sensorId             = info.sensorId;
    desc.sensorInfoData.acquisitionTime      =
        info.acquisitionTime.toString(Qt::ISODate);
    desc.sensorInfoData.solarZenithAngle     = info.solarZenithAngle;
    desc.sensorInfoData.solarAzimuthAngle    = info.solarAzimuthAngle;
    desc.sensorInfoData.sensorZenithAngle    = info.sensorZenithAngle;
    desc.sensorInfoData.sensorAzimuthAngle   = info.sensorAzimuthAngle;
    desc.sensorInfoData.earthSunDistance     = info.earthSunDistance;
    desc.sensorInfoData.quantificationValue  = info.quantificationValue;
    desc.sensorInfoData.reflectanceU         = info.reflectanceU;
    desc.sensorInfoData.nodataValue          = info.nodataValue;
    desc.sensorInfoData.saturatedValue       = info.saturatedValue;

    // 波段级元数据
    for (int i = 0; i < info.bands.size() && i < desc.bands.size(); ++i)
    {
        desc.bands[i].wavelengthMin     = info.bands[i].wavelengthMin;
        desc.bands[i].wavelengthMax     = info.bands[i].wavelengthMax;
        desc.bands[i].wavelengthCentral = info.bands[i].wavelengthCentral;
        desc.bands[i].gain              = info.bands[i].gain;
        desc.bands[i].offset            = info.bands[i].offset;
        desc.bands[i].solarIrradiance   = info.bands[i].solarIrradiance;
    }

    return desc;
}
