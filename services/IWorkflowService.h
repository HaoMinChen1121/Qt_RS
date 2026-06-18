#ifndef IWORKFLOWSERVICE_H
#define IWORKFLOWSERVICE_H

#include "services/IProcessingService.h"
#include "domain/Project.h"
#include "domain/WorkflowGraph.h"

class IWorkflowService : public IProcessingService
{
    Q_OBJECT
public:
    /// 执行工程 — 根据 Project 自动展开并运行全流程
    virtual void runProject(const Project& project) = 0;

    /// 仅展开，不执行 — 用于预览生成的 DAG
    virtual WorkflowGraph expand(const Project& project) = 0;

    /// 保存工程文件 (.rjp)
    virtual bool saveProject(const Project& project, const QString& filePath) = 0;

    /// 加载工程文件
    virtual Project loadProject(const QString& filePath) = 0;

signals:
    void nodeProgressChanged(const QString& nodeId, int percent, const QString& statusMessage);
    void stageStarted(const QString& stageId, int totalNodes);
    void nodeError(const QString& nodeId, const QString& errorMessage);
    void workflowFinished(bool success, const QString& summary);
};

#endif // IWORKFLOWSERVICE_H
