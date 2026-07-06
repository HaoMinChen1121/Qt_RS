#ifndef IVECTORREPROJECTIONSERVICE_H
#define IVECTORREPROJECTIONSERVICE_H
#include "services/IProcessingService.h"
#include "domain/params/VectorReprojectionParams.h"

class IVectorReprojectionService : public IProcessingService
{
    Q_OBJECT
public:
    virtual void execute(const VectorReprojectionParams& params) = 0;
};

#endif
