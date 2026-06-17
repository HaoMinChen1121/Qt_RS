#ifndef IRADIOMETRICSERVICE_H
#define IRADIOMETRICSERVICE_H

#include "services/IProcessingService.h"
#include "domain/params/RadiometricCorrectionParams.h"

class IRadiometricService : public IProcessingService
{
    Q_OBJECT
public:
    virtual void execute(const RadiometricCorrectionParams& params) = 0;
    virtual void executeBatch(const QList<RadiometricCorrectionParams>& batch) = 0;
};

#endif // IRADIOMETRICSERVICE_H
