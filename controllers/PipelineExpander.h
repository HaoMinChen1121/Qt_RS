#ifndef PIPELINEEXPANDER_H
#define PIPELINEEXPANDER_H

#include "domain/Project.h"
#include "domain/WorkflowGraph.h"
#include <QString>

class PipelineExpander
{
public:
    /// 根据工程自动生成执行 DAG
    WorkflowGraph expand(const Project& project);

    /// 生成默认的标准全流程
    static PipelineDefinition standardPipeline();

private:
    int mNodeCounter = 0;
    int mEdgeCounter = 0;

    QString makeNodeId(const QString& stageId, int imageIndex = -1);
    QString makeEdgeId();

    WorkflowNode createReadNode(const ImageSource& source, int index);
    WorkflowNode createPerImageNode(const PipelineStage& stage, int index);
    WorkflowNode createAllImagesNode(const PipelineStage& stage);

    void connectNode(const QString& srcId, const QString& srcPort,
                     const QString& tgtId, const QString& tgtPort,
                     WorkflowGraph& graph);
};

#endif // PIPELINEEXPANDER_H
