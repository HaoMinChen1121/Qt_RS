#ifndef IWORKFLOWSERVICE_H
#define IWORKFLOWSERVICE_H

#include "services/IProcessingService.h"
#include "domain/WorkflowGraph.h"

class IWorkflowService : public IProcessingService
{
    Q_OBJECT
public:
    virtual void run(const WorkflowGraph& graph) = 0;
    virtual bool saveTemplate(const WorkflowGraph& graph, const QString& filePath) = 0;
    virtual WorkflowGraph loadTemplate(const QString& filePath) = 0;
    virtual void previewNode(const WorkflowGraph& graph, const QString& nodeId) = 0;

signals:
    void nodeProgressChanged(const QString& nodeId, int percent, const QString& statusMessage);
    void nodeError(const QString& nodeId, const QString& errorMessage);
    void workflowFinished(bool success, const QString& summary);
};

#endif // IWORKFLOWSERVICE_H
