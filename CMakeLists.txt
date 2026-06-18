cmake_minimum_required(VERSION 3.16)
SET(VERSION_SHORT 0.2)
project(MainWindowExample VERSION ${VERSION_SHORT} LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Qt ──────────────────────────────────────────────────────────────────
find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} 5.8
    COMPONENTS Core Gui Widgets Concurrent REQUIRED)

# ── GDAL (raster I/O) ───────────────────────────────────────────────────
find_package(GDAL REQUIRED)

# ── SARibbonBar (Ribbon UI framework) ───────────────────────────────────
if(NOT TARGET SARibbonBar)
    message(STATUS "NOT TARGET SARibbonBar find_package(SARibbonBar REQUIRED)")
    find_package(SARibbonBar REQUIRED)
endif()

# ── QGIS (map canvas) ───────────────────────────────────────────────────
find_package(QGIS QUIET)

# ── CUDA (optional, for geometric resample acceleration) ─────────────────
option(ENABLE_CUDA "Enable CUDA accelerated resampling" OFF)
if(ENABLE_CUDA)
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 14)
    find_package(CUDAToolkit REQUIRED)
endif()

# ── Sources ─────────────────────────────────────────────────────────────
add_executable(MainWindowExample
    mainwindow.h
    mainwindow.cpp
    mainwindow.ui
    main.cpp
    resouce.qrc

    # === Ribbon framework (local) ===
    SARibbon.h    SARibbon.cpp

    # === 领域层：数据契约与领域对象 ===
    domain/params/RadiometricCorrectionParams.h
    domain/params/GeometricCorrectionParams.h
    domain/params/ImageFusionParams.h
    domain/params/MosaicParams.h
    domain/RasterImage.h
    domain/SensorInfo.h
    domain/BandConfiguration.h
    domain/ProductDescriptor.h
    domain/ProcessingTask.h
    domain/ProcessingReport.h
    domain/WorkflowGraph.h    domain/WorkflowGraph.cpp
    domain/ImageSource.h
    domain/PipelineDefinition.h
    domain/Project.h

    # === 业务服务接口 ===
    services/IProcessingService.h
    services/IRadiometricService.h
    services/IGeometricService.h
    services/IFusionService.h
    services/IMosaicService.h
    services/IWorkflowService.h
    services/IBatchService.h
    services/ILayerService.h

    # === 业务服务实现 ===
    services/impl/RadiometricServiceImpl.h   services/impl/RadiometricServiceImpl.cpp
    services/impl/GeometricServiceImpl.h     services/impl/GeometricServiceImpl.cpp
    services/impl/FusionServiceImpl.h        services/impl/FusionServiceImpl.cpp
    services/impl/MosaicServiceImpl.h        services/impl/MosaicServiceImpl.cpp
    services/impl/WorkflowServiceImpl.h      services/impl/WorkflowServiceImpl.cpp
    services/impl/BatchServiceImpl.h         services/impl/BatchServiceImpl.cpp
    services/impl/LayerServiceImpl.h         services/impl/LayerServiceImpl.cpp

    # === 数据访问接口 ===
    dataaccess/IRasterReader.h
    dataaccess/IRasterWriter.h
    dataaccess/IProcessingReportRepository.h
    dataaccess/IWorkflowTemplateRepository.h
    dataaccess/IProjectRepository.h
    dataaccess/ISensorProduct.h
    dataaccess/SensorProductFactory.h

    # === 数据访问实现 ===
    dataaccess/impl/GdalRasterReader.h              dataaccess/impl/GdalRasterReader.cpp
    dataaccess/impl/GdalRasterWriter.h              dataaccess/impl/GdalRasterWriter.cpp
    dataaccess/impl/JsonReportRepository.h          dataaccess/impl/JsonReportRepository.cpp
    dataaccess/impl/XmlWorkflowTemplateRepository.h dataaccess/impl/XmlWorkflowTemplateRepository.cpp
    dataaccess/impl/JsonProjectRepository.h         dataaccess/impl/JsonProjectRepository.cpp
    dataaccess/impl/Sentinel2Product.h              dataaccess/impl/Sentinel2Product.cpp
    dataaccess/impl/LandsatProduct.h                dataaccess/impl/LandsatProduct.cpp
    dataaccess/impl/GfProduct.h                     dataaccess/impl/GfProduct.cpp
    dataaccess/impl/DescriptorProduct.h             dataaccess/impl/DescriptorProduct.cpp

    # === 控制器 / 组装层 ===
    controllers/ApplicationController.h     controllers/ApplicationController.cpp
    controllers/PipelineExpander.h          controllers/PipelineExpander.cpp
    controllers/TaskWorker.h                controllers/TaskWorker.cpp
    controllers/WorkerManager.h             controllers/WorkerManager.cpp
    controllers/workers/RadiometricWorker.h controllers/workers/RadiometricWorker.cpp
    controllers/workers/GeometricWorker.h   controllers/workers/GeometricWorker.cpp
    controllers/workers/FusionWorker.h      controllers/workers/FusionWorker.cpp
    controllers/workers/MosaicWorker.h      controllers/workers/MosaicWorker.cpp

    # === 算法层：通用 ===
    algorithms/common/AlgorithmResult.h
    algorithms/common/ProgressCallback.h
    algorithms/common/GeoTransformUtils.h   algorithms/common/GeoTransformUtils.cpp

    # === 算法层：辐射定标与大气校正 ===
    algorithms/radiometric/AtmosphericCorrector.h  algorithms/radiometric/AtmosphericCorrector.cpp
    algorithms/radiometric/DnToRadiance.h          algorithms/radiometric/DnToRadiance.cpp
    algorithms/radiometric/DnToReflectance.h       algorithms/radiometric/DnToReflectance.cpp

    # === 算法层：几何校正 ===
    algorithms/geometric/GcpMatcher.h          algorithms/geometric/GcpMatcher.cpp
    algorithms/geometric/GcpModelSolver.h      algorithms/geometric/GcpModelSolver.cpp
    algorithms/geometric/GeometricCorrector.h  algorithms/geometric/GeometricCorrector.cpp
    algorithms/geometric/ResampleEngine.h      algorithms/geometric/ResampleEngine.cpp

    # === 算法层：镶嵌成图 ===
    algorithms/mosaic/HistogramMatcher.h    algorithms/mosaic/HistogramMatcher.cpp
    algorithms/mosaic/SeamlineGenerator.h   algorithms/mosaic/SeamlineGenerator.cpp
    algorithms/mosaic/FeatheringBlender.h   algorithms/mosaic/FeatheringBlender.cpp
    algorithms/mosaic/MosaicAssembler.h     algorithms/mosaic/MosaicAssembler.cpp

    # === 算法层：图像融合 ===
    algorithms/fusion/IFusionAlgorithm.h
    algorithms/fusion/IhsFusion.h              algorithms/fusion/IhsFusion.cpp
    algorithms/fusion/BroveyFusion.h           algorithms/fusion/BroveyFusion.cpp
    algorithms/fusion/GramSchmidtFusion.h      algorithms/fusion/GramSchmidtFusion.cpp
    algorithms/fusion/PcaFusion.h              algorithms/fusion/PcaFusion.cpp
    algorithms/fusion/HpfFusion.h              algorithms/fusion/HpfFusion.cpp
    algorithms/fusion/WaveletFusion.h          algorithms/fusion/WaveletFusion.cpp
    algorithms/fusion/QualityMetricsEvaluator.h algorithms/fusion/QualityMetricsEvaluator.cpp

    # === 表示层：对话框 ===
    ui/RadiometricDialog.h          ui/RadiometricDialog.cpp
    ui/GeometricDialog.h            ui/GeometricDialog.cpp
    ui/ExportDialog.h               ui/ExportDialog.cpp
    ui/FusionDialog.h               ui/FusionDialog.cpp
    ui/MosaicDialog.h               ui/MosaicDialog.cpp
    ui/BandCombinationDialog.h      ui/BandCombinationDialog.cpp
    ui/ProductBandDialog.h          ui/ProductBandDialog.cpp
    ui/BandManagerDialog.h          ui/BandManagerDialog.cpp
    ui/SpectralProfileDialog.h      ui/SpectralProfileDialog.cpp

    # === 表示层：面板与控件 ===
    ui/WorkflowPanel.h              ui/WorkflowPanel.cpp
    ui/PipelineDialog.h             ui/PipelineDialog.cpp
    ui/BatchProcessPanel.h          ui/BatchProcessPanel.cpp
    ui/LayerPanel.h                 ui/LayerPanel.cpp
    ui/MapCanvasWidget.h            ui/MapCanvasWidget.cpp
    ui/BandManagerPanel.h           ui/BandManagerPanel.cpp
    ui/RasterMetadataPanel.h        ui/RasterMetadataPanel.cpp
    ui/RadiometricCorrectionParams.h
    ui/GeometricTypes.h
    ui/ImageFusionParams.h
)

# ── CUDA sources (conditional) ───────────────────────────────────────────
if(ENABLE_CUDA)
    target_sources(MainWindowExample PRIVATE
        algorithms/geometric/cuda/ResampleKernels.cu
        algorithms/geometric/cuda/ResampleKernels.h
    )
    set_target_properties(MainWindowExample PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON)
    target_compile_definitions(MainWindowExample PRIVATE HAVE_CUDA)
endif()

# ── Windows GUI subsystem ───────────────────────────────────────────────
if(WIN32)
    set_target_properties(MainWindowExample PROPERTIES
        WIN32_EXECUTABLE TRUE)
endif()

# ── Include directories ─────────────────────────────────────────────────
target_include_directories(MainWindowExample PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/domain
    ${CMAKE_CURRENT_SOURCE_DIR}/domain/params
    ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_SOURCE_DIR}/services/impl
    ${CMAKE_CURRENT_SOURCE_DIR}/dataaccess
    ${CMAKE_CURRENT_SOURCE_DIR}/dataaccess/impl
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers/workers
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithms
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithms/common
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithms/radiometric
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithms/geometric
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithms/mosaic
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithms/fusion
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${GDAL_INCLUDE_DIR}
)

# ── Link libraries ──────────────────────────────────────────────────────
target_link_libraries(MainWindowExample PUBLIC
    SARibbonBar::SARibbonBar
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    GDAL::GDAL
)

if(QGIS_FOUND)
    target_link_libraries(MainWindowExample PUBLIC
        qgis_core qgis_gui qgis_app)
endif()

if(ENABLE_CUDA AND CUDAToolkit_FOUND)
    target_link_libraries(MainWindowExample PRIVATE
        CUDA::cudart CUDA::cuda_driver)
endif()

# ── Target properties ───────────────────────────────────────────────────
set_target_properties(MainWindowExample PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
    CXX_EXTENSIONS OFF
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
    VERSION ${SARIBBON_VERSION}
    EXPORT_NAME MainWindowExample
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ── Install ─────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS MainWindowExample
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(MainWindowExample)
endif()
