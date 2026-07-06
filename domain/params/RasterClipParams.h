#ifndef RASTERCLIPPARAMS_H
#define RASTERCLIPPARAMS_H

#include <QString>
#include <QMetaType>

struct RasterClipParams
{
    QString rasterPath;
    QString vectorPath;
    QString vectorLayerName;     // empty = first layer
    bool    cropToCutline = true;
    QString outputPath;
};

Q_DECLARE_METATYPE(RasterClipParams)

#endif // RASTERCLIPPARAMS_H
