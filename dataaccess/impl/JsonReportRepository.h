#ifndef JSONREPORTREPOSITORY_H
#define JSONREPORTREPOSITORY_H

#include "dataaccess/IProcessingReportRepository.h"

class JsonReportRepository : public IProcessingReportRepository
{
public:
    bool save(const ProcessingReport& report, const QString& filePath) override;
    ProcessingReport load(const QString& filePath) override;
    bool exportHtml(const ProcessingReport& report, const QString& filePath) override;
    bool exportText(const ProcessingReport& report, const QString& filePath) override;
};

#endif // JSONREPORTREPOSITORY_H
