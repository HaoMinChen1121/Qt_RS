#ifndef IPROCESSINGREPORTREPOSITORY_H
#define IPROCESSINGREPORTREPOSITORY_H

#include <QString>
#include "domain/ProcessingReport.h"

class IProcessingReportRepository
{
public:
    virtual ~IProcessingReportRepository() = default;

    virtual bool save(const ProcessingReport& report, const QString& filePath) = 0;
    virtual ProcessingReport load(const QString& filePath) = 0;
    virtual bool exportHtml(const ProcessingReport& report, const QString& filePath) = 0;
    virtual bool exportText(const ProcessingReport& report, const QString& filePath) = 0;
};

#endif // IPROCESSINGREPORTREPOSITORY_H
