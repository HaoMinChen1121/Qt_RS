#ifndef IRASTERREPROJECTIONSERVICE_H
#define IRASTERREPROJECTIONSERVICE_H
#include "services/IProcessingService.h"
#include "domain/params/RasterReprojectionParams.h"

class IRasterReprojectionService : public IProcessingService
{
    Q_OBJECT
public:
    virtual void execute(const RasterReprojectionParams& params) = 0;
};

#endif
