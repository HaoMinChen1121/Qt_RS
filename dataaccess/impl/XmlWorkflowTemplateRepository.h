#ifndef XMLWORKFLOWTEMPLATEREPOSITORY_H
#define XMLWORKFLOWTEMPLATEREPOSITORY_H

#include "dataaccess/IWorkflowTemplateRepository.h"

class XmlWorkflowTemplateRepository : public IWorkflowTemplateRepository
{
public:
    bool save(const WorkflowGraph& graph, const QString& filePath) override;
    WorkflowGraph load(const QString& filePath) override;
};

#endif // XMLWORKFLOWTEMPLATEREPOSITORY_H
