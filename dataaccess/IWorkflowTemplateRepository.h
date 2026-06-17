#ifndef IWORKFLOWTEMPLATEREPOSITORY_H
#define IWORKFLOWTEMPLATEREPOSITORY_H

#include <QString>
#include "domain/WorkflowGraph.h"

class IWorkflowTemplateRepository
{
public:
    virtual ~IWorkflowTemplateRepository() = default;

    virtual bool save(const WorkflowGraph& graph, const QString& filePath) = 0;
    virtual WorkflowGraph load(const QString& filePath) = 0;
};

#endif // IWORKFLOWTEMPLATEREPOSITORY_H
