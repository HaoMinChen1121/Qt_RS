#ifndef VECTORREPROJECTIONPARAMS_H
#define VECTORREPROJECTIONPARAMS_H

#include <QString>
#include <QMetaType>

struct VectorReprojectionParams
{
    QString sourcePath;
    QString targetCrsWkt;
    QString targetCrsAuthId;
    QString outputPath;
};

Q_DECLARE_METATYPE(VectorReprojectionParams)
#endif
