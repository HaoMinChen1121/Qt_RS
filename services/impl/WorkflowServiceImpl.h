#ifndef WORKFLOWSERVICEIMPL_H
#define WORKFLOWSERVICEIMPL_H

#include "services/IWorkflowService.h"

class IWorkflowTemplateRepository;

class WorkflowServiceImpl : public IWorkflowService
{
    Q_OBJECT
public:
    explicit WorkflowServiceImpl(IWorkflowTemplateRepository* repo, QObject* parent = nullptr);

    void run(const WorkflowGraph& graph) override;
    bool saveTemplate(const WorkflowGraph& graph, const QString& filePath) override;
    WorkflowGraph loadTemplate(const QString& filePath) override;
    void previewNode(const WorkflowGraph& graph, const QString& nodeId) override;
    void cancel() override;
    bool isRunning() const override;

private:
    IWorkflowTemplateRepository* mRepo;
    bool mRunning = false;
};

#endif // WORKFLOWSERVICEIMPL_H
