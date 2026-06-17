#include "WorkflowServiceImpl.h"
#include "dataaccess/IWorkflowTemplateRepository.h"
#include <QDebug>

WorkflowServiceImpl::WorkflowServiceImpl(IWorkflowTemplateRepository* repo, QObject* parent)
    : IWorkflowService(), mRepo(repo)    
{
    setParent(parent);
}

void WorkflowServiceImpl::run(const WorkflowGraph& graph)
{
    qDebug() << "[WorkflowService] run:" << graph.graphName << "nodes:" << graph.nodes.size();
    mRunning = true;
    QStringList order = graph.executionOrder();
    for (int i = 0; i < order.size(); ++i)
    {
        emit nodeProgressChanged(order[i], (i + 1) * 100 / order.size(), QStringLiteral("Running..."));
    }
    mRunning = false;
    emit workflowFinished(true, QStringLiteral("Workflow completed successfully"));
}

bool WorkflowServiceImpl::saveTemplate(const WorkflowGraph& graph, const QString& filePath)
{
    qDebug() << "[WorkflowService] saveTemplate:" << filePath;
    return mRepo->save(graph, filePath);
}

WorkflowGraph WorkflowServiceImpl::loadTemplate(const QString& filePath)
{
    qDebug() << "[WorkflowService] loadTemplate:" << filePath;
    return mRepo->load(filePath);
}

void WorkflowServiceImpl::previewNode(const WorkflowGraph& graph, const QString& nodeId)
{
    qDebug() << "[WorkflowService] previewNode:" << nodeId << "in graph:" << graph.graphName;
}

void WorkflowServiceImpl::cancel()
{
    qDebug() << "[WorkflowService] cancel";
    mRunning = false;
}

bool WorkflowServiceImpl::isRunning() const
{
    return mRunning;
}
