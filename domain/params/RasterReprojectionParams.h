#ifndef RASTERREPROJECTIONPARAMS_H
#define RASTERREPROJECTIONPARAMS_H

#include <QString>
#include <QMetaType>

struct RasterReprojectionParams
{
    QString sourcePath;
    QString targetCrsWkt;
    QString targetCrsAuthId;
    QString resampleMethod = QStringLiteral("near");
    QString outputPath;
};

Q_DECLARE_METATYPE(RasterReprojectionParams)
#endif
