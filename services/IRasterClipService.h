#ifndef IRASTERCLIPSERVICE_H
#define IRASTERCLIPSERVICE_H

#include "services/IProcessingService.h"
#include "domain/params/RasterClipParams.h"

class IRasterClipService : public IProcessingService
{
    Q_OBJECT
public:
    virtual void execute(const RasterClipParams& params) = 0;
};

#endif // IRASTERCLIPSERVICE_H
