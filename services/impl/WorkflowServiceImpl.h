#ifndef WORKFLOWSERVICEIMPL_H
#define WORKFLOWSERVICEIMPL_H

#include "services/IWorkflowService.h"
#include <QMap>
#include <QStringList>

class IRadiometricService;
class IGeometricService;
class IFusionService;
class IMosaicService;
class IWorkflowTemplateRepository;
class PipelineExpander;

class WorkflowServiceImpl : public IWorkflowService
{
    Q_OBJECT
public:
    WorkflowServiceImpl(IRadiometricService*       radiometricSvc,
                        IGeometricService*         geometricSvc,
                        IFusionService*            fusionSvc,
                        IMosaicService*            mosaicSvc,
                        IWorkflowTemplateRepository* repo,
                        QObject* parent = nullptr);

    void runProject(const Project& project) override;
    WorkflowGraph expand(const Project& project) override;
    bool saveProject(const Project& project, const QString& filePath) override;
    Project loadProject(const QString& filePath) override;
    void cancel() override;
    bool isRunning() const override;

private:
    // 内部执行上下文
    struct Context
    {
        QString userOutputDir;                    // 用户指定的输出目录
        QMap<QString, QString> nodeOutput;        // nodeId → 输出目录(或单文件)
        QMap<QString, QString> productName;       // nodeId → 产品名称
    };

    // 逐节点执行
    bool executeNode(const WorkflowNode& node,
                     const WorkflowGraph& graph,
                     Context& ctx);

    // 各节点类型的执行器
    bool execRead(const WorkflowNode& node, Context& ctx);
    bool execRadiometric(const WorkflowNode& node, const WorkflowGraph& graph, Context& ctx);
    bool execGeometric(const WorkflowNode& node, const WorkflowGraph& graph, Context& ctx);
    bool execComposite(const WorkflowNode& node, const WorkflowGraph& graph, Context& ctx);
    bool execFusion(const WorkflowNode& node, const WorkflowGraph& graph, Context& ctx);
    bool execMosaic(const WorkflowNode& node, const WorkflowGraph& graph, Context& ctx);
    bool execWrite(const WorkflowNode& node, const WorkflowGraph& graph, Context& ctx);

    // 辅助：从上游节点的输出目录中收集文件路径
    QStringList gatherInputPaths(const WorkflowNode& node,
                                 const WorkflowGraph& graph,
                                 const Context& ctx) const;

    // 辅助：向上游追溯 Read 节点的属性（sensorType / bandSelections 等）
    QVariant upstreamProperty(const WorkflowNode& node,
                              const WorkflowGraph& graph,
                              const QString& key,
                              const QVariant& defaultValue = {}) const;

    // 辅助：同步等待服务完成
    bool waitForService(QObject* service);

    IRadiometricService*        mRadiometricSvc;
    IGeometricService*          mGeometricSvc;
    IFusionService*             mFusionSvc;
    IMosaicService*             mMosaicSvc;
    IWorkflowTemplateRepository* mRepo;
    PipelineExpander*           mExpander;

    bool mRunning = false;
    bool mCancelled = false;
};

#endif // WORKFLOWSERVICEIMPL_H
