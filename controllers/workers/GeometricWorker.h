#ifndef GEOMETRICWORKER_H
#define GEOMETRICWORKER_H

#include "controllers/TaskWorker.h"
#include "domain/params/GeometricCorrectionParams.h"

class GeometricWorker : public TaskWorker
{
    Q_OBJECT

public:
    explicit GeometricWorker(const GeometricCorrectionParams& params,
                             QObject* parent = nullptr);

public slots:
    void process() override;

private:
    GeometricCorrectionParams mParams;
};

#endif // GEOMETRICWORKER_H
