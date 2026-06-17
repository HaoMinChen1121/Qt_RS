#ifndef IRASTERWRITER_H
#define IRASTERWRITER_H

#include <QString>
#include <QVector>
#include <QSize>

class IRasterWriter
{
public:
    virtual ~IRasterWriter() = default;

    virtual bool create(const QString& filePath, int width, int height, int bandCount,
                        const QString& dataType, const QVector<double>& geoTransform,
                        const QString& projectionWkt, double noDataValue = -9999.0) = 0;
    virtual bool writeBand(int bandIndex, const QVector<float>& data) = 0;
    virtual bool writeBandWindow(int bandIndex, int xOff, int yOff, int xSize, int ySize,
                                 const QVector<float>& data) = 0;
    virtual bool setBandDescription(int bandIndex, const QString& desc) = 0;
    virtual bool close() = 0;
};

#endif // IRASTERWRITER_H
