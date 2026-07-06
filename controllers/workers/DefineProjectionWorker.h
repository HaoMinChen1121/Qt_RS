#ifndef DEFINEPROJECTIONWORKER_H
#define DEFINEPROJECTIONWORKER_H
#include "controllers/TaskWorker.h"
#include "domain/params/DefineProjectionParams.h"

class DefineProjectionWorker : public TaskWorker
{
    Q_OBJECT
public:
    explicit DefineProjectionWorker(const DefineProjectionParams& p, QObject* parent = nullptr);
public slots: void process() override;
private: DefineProjectionParams mParams;
};
#endif
