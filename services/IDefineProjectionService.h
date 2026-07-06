#ifndef IDEFINEPROJECTIONSERVICE_H
#define IDEFINEPROJECTIONSERVICE_H
#include "services/IProcessingService.h"
#include "domain/params/DefineProjectionParams.h"

class IDefineProjectionService : public IProcessingService
{
    Q_OBJECT
public:
    virtual void execute(const DefineProjectionParams& params) = 0;
};

#endif
