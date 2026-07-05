#ifndef GDALVECTORREADER_H
#define GDALVECTORREADER_H

#include "dataaccess/IVectorReader.h"

class GDALDataset;

class GdalVectorReader : public IVectorReader
{
public:
    ~GdalVectorReader() override;
    bool open(const QString& filePath) override;
    void close() override;
    int layerCount() const override;
    QStringList layerNames() const override;
    int featureCount(int layerIndex = 0) const override;
    QString geometryType(int layerIndex = 0) const override;
    QString projectionWkt() const override;
    int epsgCode() const override;
    QRectF extent(int layerIndex = 0) const override;
    QStringList fieldNames(int layerIndex = 0) const override;
    QStringList fieldTypes(int layerIndex = 0) const override;
    VectorLayerInfo toVectorLayerInfo(const QString& layerId,
                                      const QString& displayName,
                                      int layerIndex = 0) const override;

    QStringList uniqueValues(int layerIndex, const QString& fieldName) const override;
    bool numericFieldRange(int layerIndex, const QString& fieldName,
                           double& minVal, double& maxVal) const override;

private:
    GDALDataset* mDataset = nullptr;
};

#endif // GDALVECTORREADER_H
