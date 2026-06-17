#ifndef IMOSAICSERVICE_H
#define IMOSAICSERVICE_H

#include "services/IProcessingService.h"
#include "domain/params/MosaicParams.h"

class IMosaicService : public IProcessingService
{
    Q_OBJECT
public:
    virtual void execute(const MosaicParams& params) = 0;
    virtual void previewSeamlines(const QStringList& imagePaths) = 0;

signals:
    void seamlinePreviewReady(const QString& previewImagePath);
};

#endif // IMOSAICSERVICE_H
