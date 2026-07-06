#ifndef DEFINEPROJECTIONPARAMS_H
#define DEFINEPROJECTIONPARAMS_H

#include <QString>
#include <QMetaType>

struct DefineProjectionParams
{
    QString sourcePath;
    QString targetCrsWkt;
    QString targetCrsAuthId;
};

Q_DECLARE_METATYPE(DefineProjectionParams)
#endif
