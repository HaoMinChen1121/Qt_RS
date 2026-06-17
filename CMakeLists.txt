cmake_minimum_required(VERSION 3.5)
SET(VERSION_SHORT 0.2)
project(MainWindowExample VERSION ${VERSION_SHORT})
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
# qt库加载，最低要求5.8
find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} 5.8 COMPONENTS Core Gui Widgets Concurrent REQUIRED)
add_executable(MainWindowExample
    mainwindow.h
    mainwindow.cpp
    mainwindow.ui
    main.cpp
    resouce.qrc

    # === 领域层：数据契约与领域对象 ===
    domain/params/RadiometricCorrectionParams.h
    domain/params/ImageFusionParams.h
    domain/params/MosaicParams.h
    domain/RasterImage.h
    domain/SensorInfo.h
    domain/ProductDescriptor.h
    domain/ProcessingTask.h
    domain/ProcessingReport.h
    domain/WorkflowGraph.h      domain/WorkflowGraph.cpp

    # === 业务服务接口 ===
    services/IProcessingService.h
    services/IRadiometricService.h
    services/IFusionService.h
    services/IMosaicService.h
    services/IWorkflowService.h
    services/IBatchService.h
    services/ILayerService.h

    # === 业务服务实现（Stub） ===
    services/impl/RadiometricServiceImpl.h  services/impl/RadiometricServiceImpl.cpp
    services/impl/FusionServiceImpl.h       services/impl/FusionServiceImpl.cpp
    services/impl/MosaicServiceImpl.h       services/impl/MosaicServiceImpl.cpp
    services/impl/WorkflowServiceImpl.h     services/impl/WorkflowServiceImpl.cpp
    services/impl/BatchServiceImpl.h        services/impl/BatchServiceImpl.cpp
    services/impl/LayerServiceImpl.h        services/impl/LayerServiceImpl.cpp

    # === 数据访问接口 ===
    dataaccess/IRasterReader.h
    dataaccess/IRasterWriter.h
    dataaccess/IProcessingReportRepository.h
    dataaccess/IWorkflowTemplateRepository.h
    dataaccess/ISensorProduct.h
    dataaccess/SensorProductFactory.h

    # === 数据访问实现（Stub） ===
    dataaccess/impl/GdalRasterReader.h      dataaccess/impl/GdalRasterReader.cpp
    dataaccess/impl/GdalRasterWriter.h      dataaccess/impl/GdalRasterWriter.cpp
    dataaccess/impl/JsonReportRepository.h  dataaccess/impl/JsonReportRepository.cpp
    dataaccess/impl/XmlWorkflowTemplateRepository.h dataaccess/impl/XmlWorkflowTemplateRepository.cpp
    dataaccess/impl/Sentinel2Product.h      dataaccess/impl/Sentinel2Product.cpp
    dataaccess/impl/LandsatProduct.h        dataaccess/impl/LandsatProduct.cpp
    dataaccess/impl/GfProduct.h             dataaccess/impl/GfProduct.cpp
    dataaccess/impl/DescriptorProduct.h     dataaccess/impl/DescriptorProduct.cpp

    # === 控制器 / 组装层 ===
    controllers/ApplicationController.h     controllers/ApplicationController.cpp
    controllers/TaskWorker.h               controllers/TaskWorker.cpp
    controllers/WorkerManager.h            controllers/WorkerManager.cpp
    controllers/workers/RadiometricWorker.h controllers/workers/RadiometricWorker.cpp
    controllers/workers/FusionWorker.h       controllers/workers/FusionWorker.cpp
    controllers/workers/MosaicWorker.h       controllers/workers/MosaicWorker.cpp

    # === 算法层 ===
    algorithms/common/AlgorithmResult.h
    algorithms/common/ProgressCallback.h
    algorithms/common/GeoTransformUtils.h   algorithms/common/GeoTransformUtils.cpp
    algorithms/radiometric/AtmosphericCorrector.h algorithms/radiometric/AtmosphericCorrector.cpp
    algorithms/radiometric/DnToRadiance.h   algorithms/radiometric/DnToRadiance.cpp
    algorithms/radiometric/DnToReflectance.h algorithms/radiometric/DnToReflectance.cpp
    # === 算法层：镶嵌成图 ===
    algorithms/mosaic/HistogramMatcher.h       algorithms/mosaic/HistogramMatcher.cpp
    algorithms/mosaic/SeamlineGenerator.h      algorithms/mosaic/SeamlineGenerator.cpp
    algorithms/mosaic/FeatheringBlender.h      algorithms/mosaic/FeatheringBlender.cpp
    algorithms/mosaic/MosaicAssembler.h        algorithms/mosaic/MosaicAssembler.cpp
    # === 算法层：图像融合 ===
    algorithms/fusion/IFusionAlgorithm.h
    algorithms/fusion/IhsFusion.h              algorithms/fusion/IhsFusion.cpp
    algorithms/fusion/BroveyFusion.h           algorithms/fusion/BroveyFusion.cpp
    algorithms/fusion/GramSchmidtFusion.h      algorithms/fusion/GramSchmidtFusion.cpp
    algorithms/fusion/PcaFusion.h              algorithms/fusion/PcaFusion.cpp
    algorithms/fusion/HpfFusion.h              algorithms/fusion/HpfFusion.cpp
    algorithms/fusion/WaveletFusion.h          algorithms/fusion/WaveletFusion.cpp
    algorithms/fusion/QualityMetricsEvaluator.h algorithms/fusion/QualityMetricsEvaluator.cpp

    # === 表示层 ===
    # 对话框类
    ui/RadiometricDialog.h
    ui/RadiometricDialog.cpp
    ui/GeometricDialog.h
    ui/GeometricDialog.cpp
    ui/ExportDialog.h
    ui/ExportDialog.cpp
    ui/FusionDialog.h
    ui/FusionDialog.cpp
    ui/MosaicDialog.h
    ui/MosaicDialog.cpp
    # 可选模块面板
    ui/WorkflowPanel.h
    ui/WorkflowPanel.cpp
    ui/BatchProcessPanel.h
    ui/BatchProcessPanel.cpp
    # 图层面板
    ui/LayerPanel.h
    ui/LayerPanel.cpp
    # 地图画布控件
    ui/MapCanvasWidget.h
    ui/MapCanvasWidget.cpp
    ui/BandCombinationDialog.h
    ui/BandCombinationDialog.cpp
    ui/ProductBandDialog.h
    ui/ProductBandDialog.cpp
)
if(WIN32)
    set_target_properties(MainWindowExample PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
if(NOT TARGET SARibbonBar)
    # 说明这个例子是单独加载
    message(STATUS "NOT TARGET SARibbonBar find_package(SARibbonBar REQUIRED)")
    find_package(SARibbonBar REQUIRED)
endif()
target_link_libraries(MainWindowExample PUBLIC SARibbonBar::SARibbonBar)
target_link_libraries(MainWindowExample PUBLIC
                                       Qt${QT_VERSION_MAJOR}::Core
                                       Qt${QT_VERSION_MAJOR}::Gui
                                       Qt${QT_VERSION_MAJOR}::Widgets)

# 添加各层 include 目录
target_include_directories(MainWindowExample PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/domain
    ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_SOURCE_DIR}/dataaccess
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithms
)

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
