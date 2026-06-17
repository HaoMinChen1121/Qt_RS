#include "GdalRasterReader.h"
#include <gdal_priv.h>
#include <ogr_srs_api.h>
#include <QDebug>
#include <cstring>

GdalRasterReader::~GdalRasterReader()
{
    close();
}

bool GdalRasterReader::open(const QString& filePath)
{
    close();

    GDALAllRegister();

    const QByteArray pathBytes = filePath.toUtf8();
    GDALDataset* ds = static_cast<GDALDataset*>(GDALOpen(pathBytes.constData(), GA_ReadOnly));
    if (!ds)
    {
        qWarning() << "[GdalRasterReader] failed to open:" << filePath;
        return false;
    }

    mDataset = ds;
    qDebug() << "[GdalRasterReader] opened:" << filePath
             << "bands:" << ds->GetRasterCount()
             << "size:" << ds->GetRasterXSize() << "x" << ds->GetRasterYSize();
    return true;
}

void GdalRasterReader::close()
{
    if (mDataset)
    {
        GDALClose(mDataset);
        mDataset = nullptr;
    }
}

QVector<float> GdalRasterReader::readBand(int bandIndex) const
{
    if (!mDataset || bandIndex < 1 || bandIndex > mDataset->GetRasterCount())
        return {};

    int w = mDataset->GetRasterXSize();
    int h = mDataset->GetRasterYSize();
    return readBandWindow(bandIndex, 0, 0, w, h);
}

QVector<float> GdalRasterReader::readBandWindow(int bandIndex, int xOff, int yOff, int xSize, int ySize) const
{
    if (!mDataset || bandIndex < 1 || bandIndex > mDataset->GetRasterCount())
        return {};

    GDALRasterBand* band = mDataset->GetRasterBand(bandIndex);
    if (!band)
        return {};

    QVector<float> data(xSize * ySize);
    CPLErr err = band->RasterIO(GF_Read, xOff, yOff, xSize, ySize,
                                 data.data(), xSize, ySize, GDT_Float32, 0, 0);
    if (err != CE_None)
    {
        qWarning() << "[GdalRasterReader] readBandWindow failed, band:" << bandIndex;
        return {};
    }
    return data;
}

int GdalRasterReader::bandCount() const
{
    return mDataset ? mDataset->GetRasterCount() : 0;
}

QSize GdalRasterReader::rasterSize() const
{
    if (!mDataset)
        return {};
    return QSize(mDataset->GetRasterXSize(), mDataset->GetRasterYSize());
}

QVector<double> GdalRasterReader::geoTransform() const
{
    if (!mDataset)
        return {0, 1, 0, 0, 0, -1};

    QVector<double> gt(6);
    if (mDataset->GetGeoTransform(gt.data()) != CE_None)
        return {0, 1, 0, 0, 0, -1};
    return gt;
}

QString GdalRasterReader::projectionWkt() const
{
    if (!mDataset)
        return {};

    const char* wkt = mDataset->GetProjectionRef();
    return wkt ? QString::fromUtf8(wkt) : QString();
}

int GdalRasterReader::epsgCode() const
{
    if (!mDataset)
        return -1;

    const char* wkt = mDataset->GetProjectionRef();
    if (!wkt || std::strlen(wkt) == 0)
        return -1;

    OGRSpatialReference srs(wkt);
    const char* code = srs.GetAuthorityCode(nullptr);
    if (code)
        return QString::fromLatin1(code).toInt();

    if (srs.AutoIdentifyEPSG() == OGRERR_NONE)
    {
        code = srs.GetAuthorityCode(nullptr);
        if (code)
            return QString::fromLatin1(code).toInt();
    }

    return -1;
}

QString GdalRasterReader::dataType() const
{
    if (!mDataset || mDataset->GetRasterCount() < 1)
        return QStringLiteral("Unknown");

    GDALRasterBand* band = mDataset->GetRasterBand(1);
    if (!band)
        return QStringLiteral("Unknown");

    const char* name = GDALGetDataTypeName(band->GetRasterDataType());
    return name ? QString::fromLatin1(name) : QStringLiteral("Unknown");
}

double GdalRasterReader::noDataValue() const
{
    if (!mDataset || mDataset->GetRasterCount() < 1)
        return -9999.0;

    GDALRasterBand* band = mDataset->GetRasterBand(1);
    if (!band)
        return -9999.0;

    int hasNoData = 0;
    double ndv = band->GetNoDataValue(&hasNoData);
    return hasNoData ? ndv : -9999.0;
}

RasterImage GdalRasterReader::toRasterImage(const QString& layerId, const QString& displayName) const
{
    RasterImage img;
    img.layerId = layerId;
    img.displayName = displayName;
    img.bandCount = bandCount();
    img.rasterSize = rasterSize();
    img.geoTransform = geoTransform();
    img.projectionWkt = projectionWkt();
    img.epsgCode = epsgCode();
    img.dataType = dataType();
    img.noDataValue = noDataValue();
    return img;
}
