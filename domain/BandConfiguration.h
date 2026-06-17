#ifndef BANDCONFIGURATION_H
#define BANDCONFIGURATION_H

#include <QString>

struct BandConfiguration
{
    QString productId;
    int redBand   = 4;    // physical band number for R channel
    int greenBand = 3;    // physical band number for G channel
    int blueBand  = 2;    // physical band number for B channel

    bool isValid() const { return redBand > 0 && greenBand > 0 && blueBand > 0; }
};

#endif // BANDCONFIGURATION_H
