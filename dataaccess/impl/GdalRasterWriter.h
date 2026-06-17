#ifndef GDALRASTERWRITER_H
#define GDALRASTERWRITER_H

#include "dataaccess/IRasterWriter.h"

class GDALDataset;

class GdalRasterWriter : public IRasterWriter
{
public:
    ~GdalRasterWriter() override;
    bool create(const QString& filePath, int width, int height, int bandCount,
                const QString& dataType, const QVector<double>& geoTransform,
                const QString& projectionWkt, double noDataValue = -9999.0) override;
    bool writeBand(int bandIndex, const QVector<float>& data) override;
    bool writeBandWindow(int bandIndex, int xOff, int yOff, int xSize, int ySize,
                         const QVector<float>& data) override;
    bool setBandDescription(int bandIndex, const QString& desc) override;
    bool close() override;

private:
    GDALDataset* mDataset = nullptr;
};

#endif // GDALRASTERWRITER_H
