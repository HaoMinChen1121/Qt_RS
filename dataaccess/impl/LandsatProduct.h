#ifndef LANDSATPRODUCT_H
#define LANDSATPRODUCT_H

#include "dataaccess/ISensorProduct.h"

class LandsatProduct : public ISensorProduct
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
    bool openFromTar(const QString& tarPath);
    SensorInfo parseMtlData(const QByteArray& data);
    SensorInfo parseMtl(const QString& mtlPath);
    static QByteArray readTextFile(const QString& vsiPath);

    QString            mOriginalPath;
    QString            mRootPath;        // /vsitar/... 根路径
    QList<RasterBandDescriptor> mBands;
    SensorInfo         mSensorInfo;
    QString            mSensorType;
    QString            mProductId;
    bool               mOpen = false;
};

#endif // LANDSATPRODUCT_H
