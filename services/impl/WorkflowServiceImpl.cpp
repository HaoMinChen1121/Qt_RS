#include "WorkflowServiceImpl.h"
#include "controllers/PipelineExpander.h"
#include "dataaccess/IWorkflowTemplateRepository.h"
#include "dataaccess/SensorProductFactory.h"
#include "services/IRadiometricService.h"
#include "services/IGeometricService.h"
#include "services/IFusionService.h"
#include "services/IMosaicService.h"
#include "domain/params/RadiometricCorrectionParams.h"
#include "domain/params/GeometricCorrectionParams.h"
#include "domain/params/ImageFusionParams.h"
#include "domain/params/MosaicParams.h"

#include <gdal_priv.h>

#include <QDir>
#include <QFile>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>

WorkflowServiceImpl::WorkflowServiceImpl(
        IRadiometricService*       radiometricSvc,
        IGeometricService*         geometricSvc,
        IFusionService*            fusionSvc,
        IMosaicService*            mosaicSvc,
        IWorkflowTemplateRepository* repo,
        QObject* parent)
    : IWorkflowService()
    , mRadiometricSvc(radiometricSvc)
    , mGeometricSvc(geometricSvc)
    , mFusionSvc(fusionSvc)
    , mMosaicSvc(mosaicSvc)
    , mRepo(repo)
    , mExpander(new PipelineExpander)
{
    setParent(parent);
}

// ────────────────────────────────────────────────────────────
// 公开接口
// ────────────────────────────────────────────────────────────
void WorkflowServiceImpl::runProject(const Project& project)
{
    if (mRunning) return;
    mRunning = true;
    mCancelled = false;

    // 1. 展开为 DAG
    WorkflowGraph graph = mExpander->expand(project);
    if (graph.nodes.isEmpty())
    {
        mRunning = false;
        emit workflowFinished(false, QStringLiteral("Pipeline expand produced empty graph"));
        return;
    }

    QString errorMsg;
    if (!graph.validate(errorMsg))
    {
        mRunning = false;
        emit workflowFinished(false, errorMsg);
        return;
    }

    // 2. 准备输出目录
    QString outDir = project.output.outputDirectory;
    if (outDir.isEmpty())
        outDir = QDir::currentPath() + QStringLiteral("/output");
    QDir().mkpath(outDir);

    Context ctx;
    ctx.userOutputDir = outDir;

    // 3. 拓扑排序
    QStringList order = graph.executionOrder();
    qDebug() << "[WorkflowService] execution order:" << order;

    // 4. 逐节点执行
    bool overallSuccess = true;
    QString finalOutputPath;

    for (int i = 0; i < order.size(); ++i)
    {
        if (mCancelled)
        {
            overallSuccess = false;
            break;
        }

        const QString& nodeId = order[i];
        const WorkflowNode* node = nullptr;
        for (const auto& n : graph.nodes)
        {
            if (n.nodeId == nodeId) { node = &n; break; }
        }
        if (!node) continue;

        emit nodeProgressChanged(nodeId, 0,
            QStringLiteral("开始: %1").arg(node->displayName));

        bool ok = executeNode(*node, graph, ctx);
        if (!ok)
        {
            overallSuccess = false;
            emit nodeError(nodeId, QStringLiteral("节点执行失败"));
            break;
        }

        // 记录 Write 节点的输出作为最终产物
        if (node->nodeType == QStringLiteral("Write") && ctx.nodeOutput.contains(nodeId))
        {
            finalOutputPath = ctx.nodeOutput[nodeId];
        }

        int pct = (i + 1) * 100 / order.size();
        emit nodeProgressChanged(nodeId, pct,
            QStringLiteral("完成: %1").arg(node->displayName));
    }

    // 5. 列出各阶段产物
    QStringList stageDirs;
    for (const auto& n : graph.nodes)
    {
        if (ctx.nodeOutput.contains(n.nodeId))
            stageDirs.append(ctx.nodeOutput[n.nodeId]);
    }

    mRunning = false;
    emit workflowFinished(overallSuccess,
        overallSuccess ? stageDirs.join(QStringLiteral("\n")) : finalOutputPath);
}

WorkflowGraph WorkflowServiceImpl::expand(const Project& project)
{
    return mExpander->expand(project);
}

bool WorkflowServiceImpl::saveProject(const Project& project, const QString& filePath)
{
    // 使用已有的 XML 模板仓库持久化内部 graph
    WorkflowGraph graph = mExpander->expand(project);

    // 将 Project 元信息注入 graph 属性
    // (WorkflowGraph 的 XML 格式用于保存 DAG，Project 信息需要额外存储)
    return mRepo->save(graph, filePath);
}

Project WorkflowServiceImpl::loadProject(const QString& filePath)
{
    // 反序列化: 从 XML 恢复 graph，但 Project 层级信息需要从 JSON 加载
    // 这里提供一个基础实现，完整实现需要 Project 专用的 JSON 仓库
    Q_UNUSED(filePath);
    qWarning() << "[WorkflowService] loadProject: use ProjectRepository for full project loading";
    return Project{};
}

void WorkflowServiceImpl::cancel()
{
    mCancelled = true;
    if (mRadiometricSvc) mRadiometricSvc->cancel();
    if (mGeometricSvc)   mGeometricSvc->cancel();
    if (mFusionSvc)      mFusionSvc->cancel();
    if (mMosaicSvc)      mMosaicSvc->cancel();
}

bool WorkflowServiceImpl::isRunning() const { return mRunning; }

// ────────────────────────────────────────────────────────────
// 节点分发
// ────────────────────────────────────────────────────────────
bool WorkflowServiceImpl::executeNode(const WorkflowNode& node,
                                       const WorkflowGraph& graph,
                                       Context& ctx)
{
    const QString& type = node.nodeType;

    if (type == QStringLiteral("Read"))        return execRead(node, ctx);
    if (type == QStringLiteral("Radiometric")) return execRadiometric(node, graph, ctx);
    if (type == QStringLiteral("Geometric"))  return execGeometric(node, graph, ctx);
    if (type == QStringLiteral("Composite"))  return execComposite(node, graph, ctx);
    if (type == QStringLiteral("Fusion"))     return execFusion(node, graph, ctx);
    if (type == QStringLiteral("Mosaic"))     return execMosaic(node, graph, ctx);
    if (type == QStringLiteral("Write"))      return execWrite(node, graph, ctx);

    // Clip / Resample — 暂委托给几何服务，后续可细化
    if (type == QStringLiteral("Clip") || type == QStringLiteral("Resample"))
        return execGeometric(node, graph, ctx);

    qWarning() << "[WorkflowService] unknown node type:" << type;
    return false;
}

// ────────────────────────────────────────────────────────────
// Read — 打开产品，提取全部波段路径
// ────────────────────────────────────────────────────────────
bool WorkflowServiceImpl::execRead(const WorkflowNode& node, Context& ctx)
{
    QString filePath = node.properties.value(QStringLiteral("filePath")).toString();
    if (filePath.isEmpty() || !QFile::exists(filePath))
    {
        emit nodeError(node.nodeId,
            QStringLiteral("影像文件不存在: %1").arg(filePath));
        return false;
    }

    QScopedPointer<ISensorProduct> prod(createSensorProduct(filePath));
    if (!prod || !prod->open(filePath))
    {
        qDebug() << "[WorkflowService] Read: pass-through:" << filePath;
        ctx.nodeOutput[node.nodeId] = filePath;
        ctx.productName[node.nodeId] = QFileInfo(filePath).completeBaseName();
        return true;
    }

    // 全部波段路径
    const auto allBands = prod->bands();
    QStringList bandPaths;
    for (const auto& bd : allBands)
        bandPaths.append(bd.rasterPath);

    // 产品名作为后续文件夹命名依据
    QString prodName = node.displayName;
    if (prodName.isEmpty())
        prodName = QFileInfo(filePath).completeBaseName();
    // 去后缀 (如 .SAFE / .zip)
    if (prodName.endsWith(QStringLiteral(".SAFE"), Qt::CaseInsensitive))
        prodName.chop(5);
    else if (prodName.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive))
        prodName.chop(4);

    ctx.productName[node.nodeId] = prodName;

    // Read 的输出是原始波段路径列表（用 ; 拼接存储）
    ctx.nodeOutput[node.nodeId] = bandPaths.join(QStringLiteral(";"));

    prod->close();
    qDebug() << "[WorkflowService] Read:" << prodName
             << "→" << bandPaths.size() << "bands";
    return true;
}

// ────────────────────────────────────────────────────────────
// Radiometric — 辐射定标 + 大气校正（一次调用完成，不分两步）
// 输出: {产品名}_RC/ (定标结果) 或 {产品名}_FLAASH/ (含大气校正)
// ────────────────────────────────────────────────────────────
bool WorkflowServiceImpl::execRadiometric(const WorkflowNode& node,
                                           const WorkflowGraph& graph,
                                           Context& ctx)
{
    QStringList inputs = gatherInputPaths(node, graph, ctx);
    if (inputs.isEmpty())
    {
        emit nodeError(node.nodeId, QStringLiteral("辐射定标: 无输入文件"));
        return false;
    }

    // 从上游节点取产品名
    QString prodName;
    for (const auto& edge : graph.edges)
    {
        if (edge.targetNodeId == node.nodeId && ctx.productName.contains(edge.sourceNodeId))
        {
            prodName = ctx.productName[edge.sourceNodeId];
            break;
        }
    }
    if (prodName.isEmpty())
        prodName = QStringLiteral("未知产品");

    bool doAtm = node.properties.value(
        QStringLiteral("doAtmosphericCorrection"), true).toBool();

    // 输出目录：取决于是否做大气校正
    QString suffix = doAtm ? QStringLiteral("_RC_FLAASH") : QStringLiteral("_RC");
    QString outDir = ctx.userOutputDir + QStringLiteral("/%1%2").arg(prodName, suffix);
    QDir().mkpath(outDir);

    RadiometricCorrectionParams params;
    params.inputFiles       = inputs;
    params.outputDirectory  = outDir;
    params.calibrationType  = node.properties.value(
        QStringLiteral("calibrationType"), QStringLiteral("DN2Radiance")).toString();
    params.outputDataType   = node.properties.value(
        QStringLiteral("outputDataType"), QStringLiteral("Float32")).toString();
    params.sensorType       = upstreamProperty(node, graph,
        QStringLiteral("sensorType")).toString();
    params.autoGainOffset   = node.properties.value(
        QStringLiteral("autoGainOffset"), true).toBool();
    params.manualGain       = node.properties.value(
        QStringLiteral("manualGain"), 1.0).toDouble();
    params.manualOffset     = node.properties.value(
        QStringLiteral("manualOffset"), 0.0).toDouble();
    params.solarZenithAngle  = node.properties.value(
        QStringLiteral("solarZenithAngle"), 0.0).toDouble();
    params.solarAzimuthAngle = node.properties.value(
        QStringLiteral("solarAzimuthAngle"), 0.0).toDouble();
    params.earthSunDistance  = node.properties.value(
        QStringLiteral("earthSunDistance"), 1.0).toDouble();
    params.metadataFile      = upstreamProperty(node, graph,
        QStringLiteral("filePath")).toString();

    if (doAtm)
    {
        params.atmModel         = node.properties.value(
            QStringLiteral("atmModel"), QStringLiteral("6S")).toString();
        params.aerosolModel     = node.properties.value(
            QStringLiteral("aerosolModel"), QStringLiteral("Continental")).toString();
        params.atmosphericModel = node.properties.value(
            QStringLiteral("atmosphericModel"), QStringLiteral("MidLatSummer")).toString();
        params.aot550       = node.properties.value(QStringLiteral("aot550"), 0.2).toDouble();
        params.waterVapor   = node.properties.value(QStringLiteral("waterVapor"), 2.0).toDouble();
        params.ozone        = node.properties.value(QStringLiteral("ozone"), 0.3).toDouble();
    }
    else
    {
        params.atmModel = QStringLiteral("None");
    }

    mRadiometricSvc->execute(params);
    bool ok = waitForService(mRadiometricSvc);

    if (ok)
    {
        ctx.nodeOutput[node.nodeId] = outDir;
        ctx.productName[node.nodeId] = prodName;
    }
    return ok;
}

// ────────────────────────────────────────────────────────────
// Geometric — 几何校正
// ────────────────────────────────────────────────────────────
bool WorkflowServiceImpl::execGeometric(const WorkflowNode& node,
                                         const WorkflowGraph& graph,
                                         Context& ctx)
{
    GeometricCorrectionParams params;

    QStringList inputs = gatherInputPaths(node, graph, ctx);
    if (inputs.isEmpty())
    {
        emit nodeError(node.nodeId, QStringLiteral("几何校正: 无输入文件"));
        return false;
    }
    params.sourceImage = inputs.first();

    params.modelType = node.properties.value(
        QStringLiteral("modelType"), QStringLiteral("Polynomial2")).toString();
    params.resampleMethod = node.properties.value(
        QStringLiteral("resampleMethod"), QStringLiteral("Bilinear")).toString();
    params.outputProjection = node.properties.value(
        QStringLiteral("outputProjection")).toString();

    QString outDir = ctx.userOutputDir + QStringLiteral("/几何校正");
    QDir().mkpath(outDir);
    QString outPath = outDir + QStringLiteral("/result.tif");
    params.outputPath = outPath;

    mGeometricSvc->execute(params);
    bool ok = waitForService(mGeometricSvc);

    if (ok)
        ctx.nodeOutput[node.nodeId] = outPath;
    return ok;
}

// ────────────────────────────────────────────────────────────
// Composite — 从上游目录中按 RGB 波段选择堆叠为 3 波段文件
// ────────────────────────────────────────────────────────────
bool WorkflowServiceImpl::execComposite(const WorkflowNode& node,
                                         const WorkflowGraph& graph,
                                         Context& ctx)
{
    // 获取上游目录
    QStringList inputs = gatherInputPaths(node, graph, ctx);
    QString inputDir;
    for (const auto& edge : graph.edges)
    {
        if (edge.targetNodeId == node.nodeId)
        {
            QString p = ctx.nodeOutput.value(edge.sourceNodeId);
            if (QFileInfo(p).isDir()) { inputDir = p; break; }
        }
    }
    if (inputDir.isEmpty() && !inputs.isEmpty())
    {
        // 上游是单个文件 → 取所在目录
        inputDir = QFileInfo(inputs.first()).absolutePath();
    }

    // 读取波段选择: 从上游 Read 节点取 bandSelections
    QVariantList bandSelList = upstreamProperty(node, graph,
        QStringLiteral("bandSelections")).toList();
    int rBand = 4, gBand = 3, bBand = 2; // 默认 RGB
    for (const auto& v : bandSelList)
    {
        QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("purpose")).toString() == QStringLiteral("Multispectral"))
        {
            QVariantList nums = m.value(QStringLiteral("bandNumbers")).toList();
            if (nums.size() >= 3)
            {
                rBand = nums[0].toInt();
                gBand = nums[1].toInt();
                bBand = nums[2].toInt();
            }
            break;
        }
    }

    // 在目录中查找对应波段文件
    auto findBandFile = [&](int bandNum) -> QString {
        QDir dir(inputDir);
        QString pattern = QStringLiteral("B%1").arg(bandNum, 2, 10, QChar('0'));
        QStringList matches = dir.entryList({pattern + QStringLiteral(".tif"),
                                              pattern + QStringLiteral(".tiff")},
                                             QDir::Files);
        if (matches.isEmpty())
        {
            // 模糊匹配: 文件名包含 B0X
            for (const QString& f : dir.entryList({QStringLiteral("*.tif"), QStringLiteral("*.tiff")},
                                                   QDir::Files))
            {
                if (f.contains(pattern, Qt::CaseInsensitive))
                    return dir.absoluteFilePath(f);
            }
            return {};
        }
        return dir.absoluteFilePath(matches.first());
    };

    QString rPath = findBandFile(rBand);
    QString gPath = findBandFile(gBand);
    QString bPath = findBandFile(bBand);

    if (rPath.isEmpty() || gPath.isEmpty() || bPath.isEmpty())
    {
        // 缺波段则透传第一条
        qWarning() << "[WorkflowService] Composite: missing bands R=" << rBand
                   << "G=" << gBand << "B=" << bBand << ", pass-through";
        ctx.nodeOutput[node.nodeId] = inputs.first();
        return true;
    }

    // 用 GDAL 堆叠为 3 波段 GeoTIFF
    QString prodName;
    for (const auto& edge : graph.edges)
    {
        if (edge.targetNodeId == node.nodeId && ctx.productName.contains(edge.sourceNodeId))
        {
            prodName = ctx.productName[edge.sourceNodeId];
            break;
        }
    }
    if (prodName.isEmpty()) prodName = QStringLiteral("composite");

    QString outDir = ctx.userOutputDir + QStringLiteral("/RGB合成");
    QDir().mkpath(outDir);
    QString outPath = outDir + QStringLiteral("/%1_RGB.tif").arg(prodName);

    // 打开参考波段获取尺寸和投影
    GDALDatasetH hRef = GDALOpen(rPath.toUtf8().constData(), GA_ReadOnly);
    if (!hRef)
    {
        emit nodeError(node.nodeId, QStringLiteral("无法打开波段文件: %1").arg(rPath));
        return false;
    }

    int width  = GDALGetRasterXSize(hRef);
    int height = GDALGetRasterYSize(hRef);
    const char* proj = GDALGetProjectionRef(hRef);
    double geo[6];
    GDALGetGeoTransform(hRef, geo);
    GDALDataType dType = GDALGetRasterDataType(GDALGetRasterBand(hRef, 1));
    GDALClose(hRef);

    GDALDriverH hDrv = GDALGetDriverByName("GTiff");
    GDALDatasetH hOut = GDALCreate(hDrv, outPath.toUtf8().constData(),
                                    width, height, 3, dType, nullptr);
    GDALSetProjection(hOut, proj);
    GDALSetGeoTransform(hOut, geo);

    // 逐波段拷贝数据
    const QString bandPaths[3] = {rPath, gPath, bPath};
    bool allOk = true;
    for (int b = 0; b < 3; ++b)
    {
        GDALDatasetH hSrc = GDALOpen(bandPaths[b].toUtf8().constData(), GA_ReadOnly);
        if (!hSrc) { allOk = false; continue; }
        GDALRasterBandH hSrcBand = GDALGetRasterBand(hSrc, 1);
        GDALRasterBandH hDstBand = GDALGetRasterBand(hOut, b + 1);

        int blockSizeX, blockSizeY;
        GDALGetBlockSize(hSrcBand, &blockSizeX, &blockSizeY);
        int rowSize = width * GDALGetDataTypeSizeBytes(dType);
        QByteArray buf(width * blockSizeY * GDALGetDataTypeSizeBytes(dType), 0);

        for (int y = 0; y < height; y += blockSizeY)
        {
            int rows = qMin(blockSizeY, height - y);
            int needBytes = width * rows * GDALGetDataTypeSizeBytes(dType);
            if (buf.size() < needBytes) buf.resize(needBytes);
            GDALRasterIO(hSrcBand, GF_Read,  0, y, width, rows,
                         buf.data(), width, rows, dType, 0, 0);
            GDALRasterIO(hDstBand, GF_Write, 0, y, width, rows,
                         buf.data(), width, rows, dType, 0, 0);
        }

        GDALClose(hSrc);
    }

    GDALClose(hOut);

    if (allOk)
    {
        ctx.nodeOutput[node.nodeId] = outPath;
        ctx.productName[node.nodeId] = prodName;
        qDebug() << "[WorkflowService] Composite:" << outPath
                 << "bands: R=" << rBand << "G=" << gBand << "B=" << bBand;
    }
    return allOk;
}

// ────────────────────────────────────────────────────────────
// Fusion — 影像融合
// ────────────────────────────────────────────────────────────
bool WorkflowServiceImpl::execFusion(const WorkflowNode& node,
                                      const WorkflowGraph& graph,
                                      Context& ctx)
{
    QString msPath, panPath;
    for (const auto& edge : graph.edges)
    {
        if (edge.targetNodeId != node.nodeId) continue;
        QString srcOutput = ctx.nodeOutput.value(edge.sourceNodeId);
        if (edge.targetPort == QStringLiteral("ms_in"))
            msPath = srcOutput;
        else if (edge.targetPort == QStringLiteral("pan_in"))
            panPath = srcOutput;
    }

    if (msPath.isEmpty() || panPath.isEmpty())
    {
        qWarning() << "[WorkflowService] Fusion skipped";
        ctx.nodeOutput[node.nodeId] = msPath.isEmpty() ? panPath : msPath;
        return true;
    }

    ImageFusionParams params;
    params.multispectralImage = msPath;
    params.panchromaticImage = panPath;
    params.algorithm = node.properties.value(
        QStringLiteral("algorithm"), QStringLiteral("GramSchmidt")).toString();

    QString outDir = ctx.userOutputDir + QStringLiteral("/影像融合");
    QDir().mkpath(outDir);
    QString outPath = outDir + QStringLiteral("/result.tif");
    params.outputPath = outPath;

    mFusionSvc->execute(params);
    bool ok = waitForService(mFusionSvc);

    if (ok)
        ctx.nodeOutput[node.nodeId] = outPath;
    return ok;
}

// ────────────────────────────────────────────────────────────
// Mosaic — 镶嵌成图
// ────────────────────────────────────────────────────────────
bool WorkflowServiceImpl::execMosaic(const WorkflowNode& node,
                                      const WorkflowGraph& graph,
                                      Context& ctx)
{
    QStringList imagePaths = gatherInputPaths(node, graph, ctx);
    if (imagePaths.size() < 2)
    {
        if (!imagePaths.isEmpty())
            ctx.nodeOutput[node.nodeId] = imagePaths.first();
        else
            emit nodeError(node.nodeId, QStringLiteral("镶嵌: 输入影像不足"));
        return !imagePaths.isEmpty();
    }

    MosaicParams params;
    params.inputImages = imagePaths;
    params.colorBalanceMethod = node.properties.value(
        QStringLiteral("colorBalanceMethod"), QStringLiteral("HistogramMatching")).toString();
    params.seamlineMethod = node.properties.value(
        QStringLiteral("seamlineMethod"), QStringLiteral("Voronoi")).toString();
    params.featheringWidth = node.properties.value(
        QStringLiteral("featheringWidth"), 10).toInt();
    params.outputFormat = node.properties.value(
        QStringLiteral("outputFormat"), QStringLiteral("GeoTIFF")).toString();

    QString outDir = ctx.userOutputDir + QStringLiteral("/镶嵌成图");
    QDir().mkpath(outDir);
    QString outPath = outDir + QStringLiteral("/result.tif");
    params.outputPath = outPath;

    mMosaicSvc->execute(params);
    bool ok = waitForService(mMosaicSvc);

    if (ok)
        ctx.nodeOutput[node.nodeId] = outPath;
    return ok;
}

// ────────────────────────────────────────────────────────────
// Write — 输出最终结果
// ────────────────────────────────────────────────────────────
bool WorkflowServiceImpl::execWrite(const WorkflowNode& node,
                                     const WorkflowGraph& graph,
                                     Context& ctx)
{
    QStringList inputs = gatherInputPaths(node, graph, ctx);
    if (inputs.isEmpty())
    {
        emit nodeError(node.nodeId, QStringLiteral("输出: 无输入文件"));
        return false;
    }

    QString outputDir = ctx.userOutputDir;
    QDir().mkpath(outputDir);

    QString ext = QStringLiteral(".tif");
    QString finalPath = outputDir + QStringLiteral("/result") + ext;

    if (QFile::exists(finalPath))
        QFile::remove(finalPath);
    bool ok = QFile::copy(inputs.first(), finalPath);

    if (ok)
        ctx.nodeOutput[node.nodeId] = finalPath;

    qDebug() << "[WorkflowService] Write:" << finalPath << "ok=" << ok;
    return ok;
}

// ────────────────────────────────────────────────────────────
// 辅助方法
// ────────────────────────────────────────────────────────────
QStringList WorkflowServiceImpl::gatherInputPaths(const WorkflowNode& node,
                                                    const WorkflowGraph& graph,
                                                    const Context& ctx) const
{
    QStringList paths;
    for (const auto& edge : graph.edges)
    {
        if (edge.targetNodeId != node.nodeId) continue;

        QString srcPath = ctx.nodeOutput.value(edge.sourceNodeId);
        if (srcPath.isEmpty()) continue;

        QFileInfo fi(srcPath);
        if (fi.isDir())
        {
            // 上游输出是目录 → 列出其中所有 .tif 文件
            QDir dir(srcPath);
            QStringList tifs = dir.entryList({QStringLiteral("*.tif"), QStringLiteral("*.tiff")},
                                              QDir::Files, QDir::Name);
            for (const QString& t : tifs)
                paths.append(dir.absoluteFilePath(t));
        }
        else if (srcPath.contains(QLatin1Char(';')))
        {
            // Read 节点的拼接波段路径列表
            for (const QString& p : srcPath.split(QLatin1Char(';')))
                if (!p.isEmpty()) paths.append(p);
        }
        else
        {
            paths.append(srcPath);
        }
    }

    if (paths.isEmpty())
    {
        QString directInput = node.properties.value(QStringLiteral("inputFile")).toString();
        if (!directInput.isEmpty())
            paths.append(directInput);
    }

    return paths;
}

QVariant WorkflowServiceImpl::upstreamProperty(const WorkflowNode& node,
                                                const WorkflowGraph& graph,
                                                const QString& key,
                                                const QVariant& defaultValue) const
{
    // 先查自身
    if (node.properties.contains(key))
        return node.properties.value(key);

    // 沿入边向上游追溯
    for (const auto& edge : graph.edges)
    {
        if (edge.targetNodeId != node.nodeId) continue;

        for (const auto& srcNode : graph.nodes)
        {
            if (srcNode.nodeId != edge.sourceNodeId) continue;
            if (srcNode.properties.contains(key))
                return srcNode.properties.value(key);

            // 递归向上
            QVariant v = upstreamProperty(srcNode, graph, key, QVariant{});
            if (v.isValid())
                return v;
        }
    }

    return defaultValue;
}

bool WorkflowServiceImpl::waitForService(QObject* service)
{
    if (!service) return false;

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    bool ok = false;
    QString errMsg;

    auto* ips = qobject_cast<IProcessingService*>(service);
    if (!ips)
    {
        qWarning() << "[WorkflowService] waitForService: service is not IProcessingService";
        return false;
    }

    auto conn1 = QObject::connect(ips, &IProcessingService::finished,
        &loop, [&](bool success, const QString&) { ok = success; loop.quit(); });

    auto conn2 = QObject::connect(ips, &IProcessingService::errorOccurred,
        &loop, [&](const QString& msg) { errMsg = msg; ok = false; loop.quit(); });

    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]()
    {
        errMsg = QStringLiteral("服务执行超时");
        ok = false;
        loop.quit();
    });

    timeout.start(3600000); // 最长等待 1 小时（大影像处理可能很久）
    loop.exec();

    QObject::disconnect(conn1);
    QObject::disconnect(conn2);

    if (!ok && !errMsg.isEmpty())
        qWarning() << "[WorkflowService] service error:" << errMsg;

    return ok;
}
