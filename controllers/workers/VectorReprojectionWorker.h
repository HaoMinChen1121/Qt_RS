#ifndef VECTORREPROJECTIONWORKER_H
#define VECTORREPROJECTIONWORKER_H
#include "controllers/TaskWorker.h"
#include "domain/params/VectorReprojectionParams.h"

class VectorReprojectionWorker : public TaskWorker
{
    Q_OBJECT
public:
    explicit VectorReprojectionWorker(const VectorReprojectionParams& p, QObject* parent = nullptr);
public slots: void process() override;
private: VectorReprojectionParams mParams;
};
#endif
