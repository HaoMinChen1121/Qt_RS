#ifndef PROCESSINGREPORT_H
#define PROCESSINGREPORT_H

#include <QString>
#include <QList>
#include <QDateTime>
#include <QMap>
#include "ProcessingTask.h"

struct ProcessingStep
{
    QString stepName;
    QString status;             // "Success", "Failed", "Skipped"
    double durationSeconds = 0;
    QString outputPath;
    QString errorMessage;
    QMap<QString, QString> parameters;
};

struct ProcessingReport
{
    QString reportTitle;
    QDateTime generatedAt;
    int totalTasks = 0;
    int successCount = 0;
    int failureCount = 0;
    double totalDurationSeconds = 0.0;
    QList<ProcessingStep> steps;
    QList<ProcessingTask> taskSummaries;
};

#endif // PROCESSINGREPORT_H
