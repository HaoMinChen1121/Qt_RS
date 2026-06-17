#include "GdalRasterWriter.h"
#include <gdal_priv.h>
#include <cpl_string.h>
#include <cmath>
#include <QDebug>
#include <QMap>

static GDALDataType stringToGdalType(const QString& typeName)
{
    static const QMap<QString, GDALDataType> map = {
        {QStringLiteral("Byte"),    GDT_Byte},
        {QStringLiteral("UInt16"),  GDT_UInt16},
        {QStringLiteral("Int16"),   GDT_Int16},
        {QStringLiteral("UInt32"),  GDT_UInt32},
        {QStringLiteral("Int32"),   GDT_Int32},
        {QStringLiteral("Float32"), GDT_Float32},
        {QStringLiteral("Float64"), GDT_Float64},
        {QStringLiteral("CInt16"),  GDT_CInt16},
        {QStringLiteral("CInt32"),  GDT_CInt32},
        {QStringLiteral("CFloat32"),GDT_CFloat32},
        {QStringLiteral("CFloat64"),GDT_CFloat64},
    };
    return map.value(typeName, GDT_Float32);
}

GdalRasterWriter::~GdalRasterWriter()
{
    close();
}

bool GdalRasterWriter::create(const QString& filePath, int width, int height, int bandCount,
                              const QString& dataType, const QVector<double>& geoTransform,
                              const QString& projectionWkt, double noDataValue)
                              {
    close();

    GDALAllRegister();

    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!driver)
    {
        qWarning() << "[GdalRasterWriter] GTiff driver not found";
        return false;
    }

    GDALDataType gdalType = stringToGdalType(dataType);

    char** options = nullptr;
    options = CSLSetNameValue(options, "COMPRESS", "LZW");
    options = CSLSetNameValue(options, "TILED", "YES");
    options = CSLSetNameValue(options, "BIGTIFF", "IF_NEEDED");

    const QByteArray pathBytes = filePath.toUtf8();
    mDataset = driver->Create(pathBytes.constData(), width, height, bandCount, gdalType, options);
    CSLDestroy(options);

    if (!mDataset)
    {
        qWarning() << "[GdalRasterWriter] failed to create:" << filePath;
        return false;
    }

    if (geoTransform.size() >= 6)
    {
        double gt[6];
        for (int i = 0; i < 6; ++i)
            gt[i] = geoTransform[i];
        mDataset->SetGeoTransform(gt);
    }

    if (!projectionWkt.isEmpty())
        mDataset->SetProjection(projectionWkt.toUtf8().constData());

    for (int i = 1; i <= bandCount; ++i)
    {
        GDALRasterBand* band = mDataset->GetRasterBand(i);
        if (band)
            band->SetNoDataValue(noDataValue);
    }

    qDebug() << "[GdalRasterWriter] created:" << filePath
             << "size:" << width << "x" << height << "bands:" << bandCount;
    return true;
}

bool GdalRasterWriter::writeBand(int bandIndex, const QVector<float>& data)
{
    if (!mDataset || bandIndex < 1 || bandIndex > mDataset->GetRasterCount())
        return false;

    int w = mDataset->GetRasterXSize();
    int h = mDataset->GetRasterYSize();
    return writeBandWindow(bandIndex, 0, 0, w, h, data);
}

bool GdalRasterWriter::writeBandWindow(int bandIndex, int xOff, int yOff, int xSize, int ySize,
                                       const QVector<float>& data)
                                       {
    if (!mDataset || bandIndex < 1 || bandIndex > mDataset->GetRasterCount())
        return false;

    GDALRasterBand* band = mDataset->GetRasterBand(bandIndex);
    if (!band)
        return false;

    QVector<float> buf = data;
    CPLErr err = band->RasterIO(GF_Write, xOff, yOff, xSize, ySize,
                                 buf.data(), xSize, ySize, GDT_Float32, 0, 0);
    if (err != CE_None)
    {
        qWarning() << "[GdalRasterWriter] writeBandWindow failed, band:" << bandIndex;
        return false;
    }
    return true;
}

bool GdalRasterWriter::setBandDescription(int bandIndex, const QString& desc)
{
    if (!mDataset || bandIndex < 1 || bandIndex > mDataset->GetRasterCount())
        return false;

    GDALRasterBand* band = mDataset->GetRasterBand(bandIndex);
    if (!band)
        return false;

    band->SetDescription(desc.toUtf8().constData());
    return true;
}

bool GdalRasterWriter::close()
{
    if (mDataset)
    {
        // Compute per-band statistics before closing so that downstream
        // VRT composites / GIS viewers have valid min/max for contrast
        // stretching. Without this, GDAL may estimate from a sparse sample
        // and hit "dfMax should be strictly greater than dfMin" on bands
        // with narrow dynamic range (e.g. atmospheric-correction outputs).
        int nBands = mDataset->GetRasterCount();
        for (int b = 1; b <= nBands; ++b)
        {
            GDALRasterBand* band = mDataset->GetRasterBand(b);
            if (!band) continue;

            double dfMin = 0, dfMax = 0, dfMean = 0, dfStdDev = 0;
            CPLErr e = band->ComputeStatistics(
                FALSE, &dfMin, &dfMax, &dfMean, &dfStdDev, nullptr, nullptr);
            if (!(e == CE_None && dfMax > dfMin))
            {
                e = band->ComputeStatistics(
                    TRUE, &dfMin, &dfMax, &dfMean, &dfStdDev, nullptr, nullptr);
            }
            if (e == CE_None && dfMax > dfMin)
            {
                band->SetStatistics(dfMin, dfMax, dfMean, dfStdDev);
            }
            else if (e == CE_None)
            {
                // dfMin == dfMax: 像素值完全相同，用合理的最小范围替代微观 epsilon
                double eps;
                if (dfMin == dfMax)
                {
                    GDALDataType dt = band->GetRasterDataType();
                    bool isFloat = (dt == GDT_Float32 || dt == GDT_Float64
                                    || dt == GDT_CFloat32 || dt == GDT_CFloat64);
                    eps = isFloat ? 1e-4 : 1.0;
                }
                else
                {
                    eps = std::max(std::fabs(dfMax) * 1e-6, 1e-12);
                }
                band->SetStatistics(dfMin - eps, dfMax + eps, dfMean, dfStdDev);
            }
            else
            {
                GDALDataType dt = band->GetRasterDataType();
                bool isFloat = (dt == GDT_Float32 || dt == GDT_Float64
                                || dt == GDT_CFloat32 || dt == GDT_CFloat64);
                double eps = isFloat ? 1e-4 : 1.0;
                band->SetStatistics(0.0 - eps, 0.0 + eps, 0.0, 0.0);
            }
        }

        GDALClose(mDataset);
        mDataset = nullptr;
        qDebug() << "[GdalRasterWriter] closed";
    }
    return true;
}
