#ifndef APPLICATIONCONTROLLER_H
#define APPLICATIONCONTROLLER_H

#include <QObject>
#include <memory>

// Service interfaces
class IRadiometricService;
class IFusionService;
class IMosaicService;
class IGeometricService;
class IWorkflowService;
class IBatchService;
class ILayerService;
class IRasterClipService;
class IRasterReprojectionService;
class IVectorReprojectionService;
class IDefineProjectionService;

// Data access interfaces
class IRasterReader;
class IRasterWriter;
class IVectorReader;
class IProcessingReportRepository;
class IWorkflowTemplateRepository;
class IProjectRepository;

class MainWindow;
class WorkerManager;
class PipelineDialog;

/**
 * @brief 组合根 / DI 容器 — 唯一同时引用 UI 和业务层的类
 *
 * 在 main() 中创建，initialize() 中创建所有服务实现和数据访问实现，
 * 并通过 wire*() 方法建立 UI 信号与业务层方法的连接。
 */
class ApplicationController : public QObject
{
    Q_OBJECT
public:
    explicit ApplicationController(MainWindow* mainWindow, QObject* parent = nullptr);
    ~ApplicationController();

    void initialize();
    void shutdown();

private:
    void createServices();
    void createDataAccess();
    void wireLayerSignals();
    void wireMapSignals();
    void wireRadiometricSignals();
    void wireFusionSignals();
    void wireMosaicSignals();
    void wireGeometricSignals();
    void wireWorkflowSignals();
    void wireToolBoxSignals();
    void wireGeneralSignals();

    MainWindow* mMainWindow;

    // Service implementations (owned)
    std::unique_ptr<IRadiometricService>  mRadiometricSvc;
    std::unique_ptr<IFusionService>       mFusionSvc;
    std::unique_ptr<IMosaicService>       mMosaicSvc;
    std::unique_ptr<IGeometricService>   mGeometricSvc;
    std::unique_ptr<IWorkflowService>    mWorkflowSvc;
    std::unique_ptr<IBatchService>        mBatchSvc;
    std::unique_ptr<ILayerService>        mLayerSvc;
    std::unique_ptr<IRasterClipService>   mRasterClipSvc;
    std::unique_ptr<IRasterReprojectionService> mRasterReprojSvc;
    std::unique_ptr<IVectorReprojectionService> mVectorReprojSvc;
    std::unique_ptr<IDefineProjectionService>   mDefineProjSvc;

    // Data access implementations (owned)
    std::unique_ptr<IRasterReader>              mRasterReader;
    std::unique_ptr<IRasterWriter>              mRasterWriter;
    std::unique_ptr<IVectorReader>              mVectorReader;
    std::unique_ptr<IProcessingReportRepository> mReportRepo;
    std::unique_ptr<IWorkflowTemplateRepository> mWorkflowRepo;
    std::unique_ptr<IProjectRepository>         mProjectRepo;

    // Worker management
    std::unique_ptr<WorkerManager> mWorkerManager;

    // UI (non-owned, created by MainWindow)
    PipelineDialog* mPipelineDialog = nullptr;

    // 流程模式下抑制弹窗
    bool mPipelineRunning = false;
};

#endif // APPLICATIONCONTROLLER_H
