#ifndef GFPRODUCT_H
#define GFPRODUCT_H

#include "dataaccess/ISensorProduct.h"

class GfProduct : public ISensorProduct
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
    SensorInfo parseGfXml(const QString& xmlPath) const;
    QString guessSensorType(const QString& dirName, const QStringList& tifFiles) const;

    QString            mOriginalPath;
    QList<RasterBandDescriptor> mBands;
    SensorInfo         mSensorInfo;
    QString            mSensorType;
    QString            mProductId;
    bool               mOpen = false;
};

#endif // GFPRODUCT_H
