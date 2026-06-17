#ifndef MOSAICWORKER_H
#define MOSAICWORKER_H

#include "controllers/TaskWorker.h"
#include "domain/params/MosaicParams.h"

class MosaicWorker : public TaskWorker
{
    Q_OBJECT
public:
    explicit MosaicWorker(const MosaicParams& params,
                           QObject* parent = nullptr);

public slots:
    void process() override;

private:
    MosaicParams mParams;
};

#endif // MOSAICWORKER_H
