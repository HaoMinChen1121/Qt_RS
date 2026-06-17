#ifndef GDALRASTERREADER_H
#define GDALRASTERREADER_H

#include "dataaccess/IRasterReader.h"

class GDALDataset;

class GdalRasterReader : public IRasterReader
{
public:
    ~GdalRasterReader() override;
    bool open(const QString& filePath) override;
    void close() override;
    QVector<float> readBand(int bandIndex) const override;
    QVector<float> readBandWindow(int bandIndex, int xOff, int yOff, int xSize, int ySize) const override;
    int bandCount() const override;
    QSize rasterSize() const override;
    QVector<double> geoTransform() const override;
    QString projectionWkt() const override;
    int epsgCode() const override;
    QString dataType() const override;
    double noDataValue() const override;
    RasterImage toRasterImage(const QString& layerId, const QString& displayName) const override;

private:
    GDALDataset* mDataset = nullptr;
};

#endif // GDALRASTERREADER_H
