#include "PipelineExpander.h"
#include <QDebug>
#include <QUuid>

// ────────────────────────────────────────────────────────────
// 默认标准全流程
// ────────────────────────────────────────────────────────────
PipelineDefinition PipelineExpander::standardPipeline()
{
    PipelineDefinition def;
    def.name = QStringLiteral("标准全流程");
    def.description = QStringLiteral("辐射定标 → 大气校正 → 几何校正 → 镶嵌成图 → 输出");

    def.stages = {
        { QStringLiteral("Read"),       QStringLiteral("读取影像"),   StageScope::PerImage,  {}, true,  true  },
        { QStringLiteral("Radiometric"),QStringLiteral("辐射定标与大气校正"), StageScope::PerImage,
            {{QStringLiteral("calibrationType"), QStringLiteral("DN2Radiance")},
             {QStringLiteral("outputDataType"), QStringLiteral("Float32")},
             {QStringLiteral("autoGainOffset"), true},
             {QStringLiteral("doAtmosphericCorrection"), true},
             {QStringLiteral("atmModel"), QStringLiteral("6S")},
             {QStringLiteral("aerosolModel"), QStringLiteral("Continental")},
             {QStringLiteral("atmosphericModel"), QStringLiteral("MidLatSummer")},
             {QStringLiteral("aot550"), 0.2},
             {QStringLiteral("waterVapor"), 2.0},
             {QStringLiteral("ozone"), 0.3}},  true, false },
        { QStringLiteral("Geometric"),  QStringLiteral("几何校正"),   StageScope::PerImage,
            {{QStringLiteral("modelType"), QStringLiteral("Polynomial2")},
             {QStringLiteral("resampleMethod"), QStringLiteral("Bilinear")}},  true, false },
        { QStringLiteral("Composite"),  QStringLiteral("RGB合成"),    StageScope::PerImage,  {}, true, false },
        { QStringLiteral("Fusion"),     QStringLiteral("影像融合"),   StageScope::PerImage,
            {{QStringLiteral("algorithm"), QStringLiteral("GramSchmidt")}},  false, false },
        { QStringLiteral("Mosaic"),     QStringLiteral("镶嵌成图"),   StageScope::AllImages,
            {{QStringLiteral("colorBalanceMethod"), QStringLiteral("HistogramMatching")},
             {QStringLiteral("seamlineMethod"), QStringLiteral("Voronoi")},
             {QStringLiteral("featheringWidth"), 10}},  true, false },
        { QStringLiteral("Write"),      QStringLiteral("输出影像"),   StageScope::AllImages,
            {{QStringLiteral("outputFormat"), QStringLiteral("GeoTIFF")}},  true, true },
    };
    return def;
}

// ────────────────────────────────────────────────────────────
// ID 生成
// ────────────────────────────────────────────────────────────
QString PipelineExpander::makeNodeId(const QString& stageId, int imageIndex)
{
    if (imageIndex < 0)
        return QStringLiteral("%1").arg(stageId);          // AllImages: "Mosaic", "Write"
    return QStringLiteral("%1_%2").arg(stageId).arg(imageIndex); // PerImage: "Read_0"
}

QString PipelineExpander::makeEdgeId()
{
    return QStringLiteral("e%1").arg(mEdgeCounter++);
}

// ────────────────────────────────────────────────────────────
// 节点工厂
// ────────────────────────────────────────────────────────────
WorkflowNode PipelineExpander::createReadNode(const ImageSource& source, int index)
{
    WorkflowNode node;
    node.nodeId      = makeNodeId(QStringLiteral("Read"), index);
    node.nodeType    = QStringLiteral("Read");
    node.displayName = source.displayName.isEmpty()
                       ? QStringLiteral("影像 %1").arg(index + 1)
                       : source.displayName;
    node.posX = 50.0;
    node.posY = 50.0 + index * 80.0;
    node.properties[QStringLiteral("filePath")]   = source.filePath;
    node.properties[QStringLiteral("sensorType")] = source.sensorType;
    node.properties[QStringLiteral("imageRole")]  = static_cast<int>(source.role);

    // 序列化波段选择到 properties
    QVariantList bandSelList;
    for (const auto& bs : source.bandSelections)
    {
        QVariantMap m;
        m[QStringLiteral("purpose")] = bs.purpose;
        QVariantList nums;
        for (int b : bs.bandNumbers)
            nums.append(b);
        m[QStringLiteral("bandNumbers")] = nums;
        bandSelList.append(m);
    }
    node.properties[QStringLiteral("bandSelections")] = bandSelList;
    node.properties[QStringLiteral("metadata")] = source.metadata;

    return node;
}

WorkflowNode PipelineExpander::createPerImageNode(const PipelineStage& stage, int index)
{
    WorkflowNode node;
    node.nodeId      = makeNodeId(stage.stageId, index);
    node.nodeType    = stage.stageId;
    node.displayName = QStringLiteral("%1 [%2]").arg(stage.displayName).arg(index + 1);
    node.posX = 50.0;
    node.posY = 50.0 + index * 80.0;
    node.properties = stage.params;
    return node;
}

WorkflowNode PipelineExpander::createAllImagesNode(const PipelineStage& stage)
{
    WorkflowNode node;
    node.nodeId      = makeNodeId(stage.stageId);
    node.nodeType    = stage.stageId;
    node.displayName = stage.displayName;
    node.posX = 50.0;
    node.posY = 50.0;
    node.properties = stage.params;
    return node;
}

void PipelineExpander::connectNode(const QString& srcId, const QString& srcPort,
                                    const QString& tgtId, const QString& tgtPort,
                                    WorkflowGraph& graph)
{
    WorkflowEdge edge;
    edge.edgeId       = makeEdgeId();
    edge.sourceNodeId = srcId;
    edge.sourcePort   = srcPort;
    edge.targetNodeId = tgtId;
    edge.targetPort   = tgtPort;
    graph.edges.append(edge);
}

// ────────────────────────────────────────────────────────────
// 核心展开逻辑
// ────────────────────────────────────────────────────────────
WorkflowGraph PipelineExpander::expand(const Project& project)
{
    mNodeCounter = 0;
    mEdgeCounter = 0;

    WorkflowGraph graph;
    graph.graphName = project.projectName;
    graph.description = project.description;

    // 分离影像角色
    QList<ImageSource> mosaicInputs;
    QList<ImageSource> panInputs;
    QList<ImageSource> refInputs;
    for (const auto& src : project.imageSources)
    {
        switch (src.role)
        {
        case ImageRole::MosaicInput:  mosaicInputs.append(src); break;
        case ImageRole::Panchromatic: panInputs.append(src);    break;
        case ImageRole::Reference:
        case ImageRole::HistReference: refInputs.append(src);   break;
        }
    }

    const int N = mosaicInputs.size();
    if (N == 0)
    {
        qWarning() << "[PipelineExpander] no MosaicInput sources";
        return graph;
    }

    // 收集启用的阶段
    QList<PipelineStage> enabledStages;
    for (const auto& s : project.pipeline.stages)
    {
        if (s.enabled)
            enabledStages.append(s);
    }

    // ── 逐阶段展开 ──
    // frontier[i] = 第 i 个处理通道的当前末端节点 ID 列表
    QList<QStringList> frontier;   // frontier.size() == N
    bool hasPanLane = false;
    int  panLaneIndex = -1;

    for (int stageIdx = 0; stageIdx < enabledStages.size(); ++stageIdx)
    {
        const PipelineStage& stage = enabledStages[stageIdx];

        if (stage.scope == StageScope::PerImage)
        {
            // ── 逐景阶段 ──
            QList<QStringList> newFrontier;

            // 检查此阶段是否需要特殊处理
            bool isFusion      = (stage.stageId == QStringLiteral("Fusion"));
            bool isRead        = (stage.stageId == QStringLiteral("Read"));
            bool isRadiometric = (stage.stageId == QStringLiteral("Radiometric"));

            // ---- Read 阶段：为每个 MosaicInput 源创建 Read 节点 ----
            if (isRead)
            {
                for (int i = 0; i < N; ++i)
                {
                    WorkflowNode node = createReadNode(mosaicInputs[i], i);
                    graph.nodes.append(node);
                    newFrontier.append({node.nodeId});
                }

                // 为全色影像也创建 Read 节点（排在 MosaicInput 之后）
                if (!panInputs.isEmpty())
                {
                    hasPanLane = true;
                    panLaneIndex = N;
                    WorkflowNode panNode = createReadNode(panInputs[0], N);
                    panNode.nodeType = QStringLiteral("Read");
                    panNode.displayName = QStringLiteral("全色影像");
                    graph.nodes.append(panNode);
                    newFrontier.append({panNode.nodeId});
                }
            }
            // ---- Fusion 阶段：需要 MS + Pan 两个输入 ----
            else if (isFusion && hasPanLane)
            {
                for (int i = 0; i < N; ++i)
                {
                    WorkflowNode node = createPerImageNode(stage, i);
                    node.nodeType = QStringLiteral("Fusion");
                    graph.nodes.append(node);

                    // MS 输入
                    for (const QString& srcId : frontier[i])
                        connectNode(srcId, QStringLiteral("out"), node.nodeId, QStringLiteral("ms_in"), graph);
                    // Pan 输入
                    for (const QString& panId : frontier[panLaneIndex])
                        connectNode(panId, QStringLiteral("out"), node.nodeId, QStringLiteral("pan_in"), graph);

                    newFrontier.append({node.nodeId});
                }
                // Pan 通道在 Fusion 后不再延续
                hasPanLane = false;
                panLaneIndex = -1;
            }
            // ---- Radiometric: 定标 + 大气校正（一次调用完成） ----
            else if (isRadiometric)
            {
                int laneCount = frontier.size();
                for (int i = 0; i < laneCount; ++i)
                {
                    WorkflowNode node = createPerImageNode(stage, i);
                    node.nodeId   = makeNodeId(QStringLiteral("Radiometric"), i);
                    node.nodeType = QStringLiteral("Radiometric");
                    node.displayName = QStringLiteral("辐射定标与大气校正 [%1]").arg(i + 1);
                    graph.nodes.append(node);

                    for (const QString& srcId : frontier[i])
                        connectNode(srcId, QStringLiteral("out"), node.nodeId, QStringLiteral("in"), graph);

                    newFrontier.append({node.nodeId});
                }
            }
            else
            {
                // 普通 PerImage 阶段（Geometric / Clip / Resample）
                int laneCount = frontier.size();
                for (int i = 0; i < laneCount; ++i)
                {
                    WorkflowNode node = createPerImageNode(stage, i);
                    graph.nodes.append(node);

                    for (const QString& srcId : frontier[i])
                        connectNode(srcId, QStringLiteral("out"), node.nodeId, QStringLiteral("in"), graph);

                    newFrontier.append({node.nodeId});
                }
            }

            frontier = newFrontier;
        }
        else
        {
            // ── 全量阶段（Mosaic / Write）──
            WorkflowNode node = createAllImagesNode(stage);
            graph.nodes.append(node);

            // 所有通道的末端节点 → 此全量节点的 "images_in" 端口
            for (int i = 0; i < frontier.size(); ++i)
            {
                for (const QString& srcId : frontier[i])
                    connectNode(srcId, QStringLiteral("out"), node.nodeId,
                                QStringLiteral("images_in"), graph);
            }

            // Write 阶段注入输出路径
            if (stage.stageId == QStringLiteral("Write"))
            {
                node.properties[QStringLiteral("outputDirectory")] = project.output.outputDirectory;
                node.properties[QStringLiteral("outputFormat")]    = project.output.outputFormat;
                node.properties[QStringLiteral("namingPattern")]   = project.output.namingPattern;
                node.properties[QStringLiteral("cleanup")]         = project.output.cleanupIntermediates;
            }

            // 全量阶段之后只有一条通道
            frontier = {{node.nodeId}};
        }
    }

    qDebug() << "[PipelineExpander] expanded" << project.projectName
             << "→ nodes:" << graph.nodes.size()
             << "edges:" << graph.edges.size();
    return graph;
}
