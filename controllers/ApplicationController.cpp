#include "ApplicationController.h"
#include "controllers/WorkerManager.h"
#include "controllers/PipelineExpander.h"

#include <ogr_srs_api.h>

// Service implementations
#include "services/impl/RadiometricServiceImpl.h"
#include "services/impl/FusionServiceImpl.h"
#include "services/impl/MosaicServiceImpl.h"
#include "services/impl/GeometricServiceImpl.h"
#include "services/impl/WorkflowServiceImpl.h"
#include "services/impl/BatchServiceImpl.h"
#include "services/impl/LayerServiceImpl.h"
#include "services/impl/RasterClipServiceImpl.h"
#include "services/impl/RasterReprojectionServiceImpl.h"
#include "services/impl/VectorReprojectionServiceImpl.h"
#include "services/impl/DefineProjectionServiceImpl.h"

// Data access implementations
#include "dataaccess/SensorProductFactory.h"
#include "dataaccess/impl/GdalRasterReader.h"
#include "dataaccess/impl/GdalRasterWriter.h"
#include "dataaccess/impl/GdalVectorReader.h"
#include "dataaccess/impl/JsonReportRepository.h"
#include "dataaccess/impl/XmlWorkflowTemplateRepository.h"
#include "dataaccess/impl/JsonProjectRepository.h"

// Domain
#include "domain/params/GeometricCorrectionParams.h"
#include "algorithms/common/GeoTransformUtils.h"
// UI
#include "mainwindow.h"
#include "ui/SpectralProfileDialog.h"
#include "ui/LayerPanel.h"
#include "ui/RasterMetadataPanel.h"
#include "ui/VectorMetadataPanel.h"
#include "ui/VectorStyleDialog.h"
#include "ui/MapCanvasWidget.h"
#include "ui/BandManagerPanel.h"
#include "ui/BandCombinationDialog.h"
#include "ui/ProductBandDialog.h"
#include "ui/ExportDialog.h"
#include "ui/PipelineDialog.h"
#include "ui/ToolBoxPanel.h"
#include "ui/RasterClipDialog.h"
#include "ui/RasterReprojectionDialog.h"
#include "ui/VectorReprojectionDialog.h"
#include "ui/DefineProjectionDialog.h"
#include "domain/params/RasterClipParams.h"
#include "domain/params/RasterReprojectionParams.h"
#include "domain/params/VectorReprojectionParams.h"
#include "domain/params/DefineProjectionParams.h"

#include <qgsmapcanvas.h>
#include <qgsrectangle.h>
#include <qgspointxy.h>
#include <qgsvertexmarker.h>
#include <qgsvectorlayer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsfillsymbol.h>
#include <qgslinesymbol.h>
#include <qgslinesymbollayer.h>
#include <qgsmarkersymbol.h>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QProgressDialog>
#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <cpl_conv.h>

ApplicationController::ApplicationController(MainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , mMainWindow(mainWindow)
    {
}

ApplicationController::~ApplicationController()
{
    shutdown();
}

void ApplicationController::initialize()
{
    qDebug() << "[ApplicationController] Initializing...";

    // Increase GDAL block cache for better raster I/O performance
    CPLSetConfigOption("GDAL_CACHEMAX", "512");
    CPLSetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");

    mWorkerManager = std::make_unique<WorkerManager>(this);

    createDataAccess();
    createServices();
    wireLayerSignals();
    wireMapSignals();
    wireRadiometricSignals();
    wireFusionSignals();
    wireMosaicSignals();
    wireGeometricSignals();
    wireWorkflowSignals();
    wireToolBoxSignals();
    wireGeneralSignals();

    qDebug() << "[ApplicationController] Initialization complete";
}

void ApplicationController::shutdown()
{
    qDebug() << "[ApplicationController] Shutting down...";
    if (mWorkerManager)
        mWorkerManager->shutdownAll();
}

void ApplicationController::createDataAccess()
{
    mRasterReader     = std::make_unique<GdalRasterReader>();
    mRasterWriter     = std::make_unique<GdalRasterWriter>();
    mVectorReader     = std::make_unique<GdalVectorReader>();
    mReportRepo       = std::make_unique<JsonReportRepository>();
    mWorkflowRepo     = std::make_unique<XmlWorkflowTemplateRepository>();
    mProjectRepo      = std::make_unique<JsonProjectRepository>();
}

void ApplicationController::createServices()
{
    mRadiometricSvc = std::make_unique<RadiometricServiceImpl>(
        mRasterReader.get(), mRasterWriter.get(), mWorkerManager.get());
    mFusionSvc      = std::make_unique<FusionServiceImpl>(
        mRasterReader.get(), mRasterWriter.get(), mWorkerManager.get());
    mMosaicSvc      = std::make_unique<MosaicServiceImpl>(mWorkerManager.get());
    mGeometricSvc   = std::make_unique<GeometricServiceImpl>(mWorkerManager.get());
    mWorkflowSvc    = std::make_unique<WorkflowServiceImpl>(
        mRadiometricSvc.get(), mGeometricSvc.get(),
        mFusionSvc.get(), mMosaicSvc.get(),
        mWorkflowRepo.get());
    mBatchSvc       = std::make_unique<BatchServiceImpl>();
    mLayerSvc       = std::make_unique<LayerServiceImpl>(mRasterReader.get(), mVectorReader.get());
    mRasterClipSvc   = std::make_unique<RasterClipServiceImpl>(mWorkerManager.get());
    mRasterReprojSvc = std::make_unique<RasterReprojectionServiceImpl>(mWorkerManager.get());
    mVectorReprojSvc = std::make_unique<VectorReprojectionServiceImpl>(mWorkerManager.get());
    mDefineProjSvc   = std::make_unique<DefineProjectionServiceImpl>(mWorkerManager.get());
}

void ApplicationController::wireLayerSignals()
{
    auto* panel = mMainWindow->layerPanel();
    if (!panel)
    {
        qWarning() << "[ApplicationController] LayerPanel not found";
        return;
    }
    auto* svc = mLayerSvc.get();

    // Layer Panel → Band Manager toggle button
    auto* bmPanel = mMainWindow->bandManagerPanel();
    connect(panel, &LayerPanel::bandManagerToggleRequested, this, [bmPanel]()
    {
        bmPanel->setVisible(!bmPanel->isVisible());
    });

    // UI → Service：栅格图层 → 多波段产品添加到波段管理器，其余直接加载
    connect(panel, &LayerPanel::layerAddRequested, this, [this, svc, bmPanel](const QStringList& paths)
    {
        QStringList remainingPaths;

        for (const QString& path : paths)
        {
            QScopedPointer<ISensorProduct> prod(createSensorProduct(path));
            if (prod && prod->open(path))
            {
                const auto bands = prod->bands();
                if (bands.size() > 1)
                {
                    bmPanel->addProduct(path, bands, prod->sensorInfo());
                    bmPanel->show();
                    continue;
                }
            }
            remainingPaths.append(path);
        }

        if (!remainingPaths.isEmpty())
            svc->addLayers(remainingPaths);
    });

    // UI → Service：矢量图层 → 直接加载
    connect(panel, &LayerPanel::vectorLayerAddRequested, this,
        [svc](const QStringList& paths)
    {
        svc->addVectorLayers(paths);
    });

    // Band Manager → open product
    connect(bmPanel, &BandManagerPanel::productOpenRequested, this,
        [this](const QString& path) { emit mMainWindow->layerPanel()->layerAddRequested({path}); });

    // Band Manager → apply RGB to canvas
    connect(bmPanel, &BandManagerPanel::applyRgbRequested, this,
        [svc](const BandConfiguration& cfg, const QString& rPath, const QString& gPath,
              const QString& bPath, const SensorInfo& info, const QString& productId)
    {
        Q_UNUSED(cfg);
        svc->addRgbLayerFromPaths(rPath, gPath, bPath, info, productId);
    });
    connect(panel, &LayerPanel::layerRemoveRequested,
            svc,   &ILayerService::removeLayers);
    connect(panel, &LayerPanel::layerVisibilityChanged,
            svc,   &ILayerService::setLayerVisibility);
    connect(panel, &LayerPanel::layerOrderChanged,
            svc,   &ILayerService::reorderLayers);
    // Service → UI
    connect(svc, &ILayerService::layerLoaded,
            panel, &LayerPanel::onLayerLoaded);
    connect(svc, &ILayerService::vectorLayerLoaded,
            panel, &LayerPanel::onVectorLayerLoaded);
    connect(svc, &ILayerService::layerRemoved,
            panel, &LayerPanel::onLayerRemoved);
    connect(svc, &ILayerService::layerError,
            panel, &LayerPanel::onLayerError);
    connect(svc, &ILayerService::renderLayersChanged, this, [this](const QList<RasterImage>&)
    {
        Q_UNUSED(this);
    });
    connect(svc, &ILayerService::vectorRenderLayersChanged, this, [this](const QList<VectorLayerInfo>&)
    {
        Q_UNUSED(this);
    });

    // Zoom to layer — use QgsMapLayer::extent() to preserve CRS
    connect(panel, &LayerPanel::zoomToLayerRequested, this, [this, svc](const QString& layerId)
    {
        QgsMapLayer* ml = svc->mapLayer(layerId);
        if (ml && !ml->extent().isEmpty())
            mMainWindow->mapCanvasWidget()->setCanvasExtent(ml->extent());
    });

    // 波段拆分
    connect(panel, &LayerPanel::splitBandsRequested, this, [svc](const QString& layerId)
    {
        svc->splitToBands(layerId);
    });

    // 波段组合 — 多波段文件直接切换渲染器; 单波段产品用 VRT 合成
    connect(panel, &LayerPanel::bandCombinationRequested, this, [this, svc](const QString& layerId)
    {
        int bc = svc->bandCount(layerId);
        if (bc >= 3)
        {
            BandCombinationDialog dlg(bc, svc->sensorTypeForLayer(layerId), mMainWindow);
            if (dlg.exec() == QDialog::Accepted)
                svc->setBandRenderer(layerId, dlg.redBand(), dlg.greenBand(), dlg.blueBand());
            return;
        }

        // 单波段图层 → 检查是否属于某个产品
        RasterImage img = svc->layerImage(layerId);
        if (img.productId.isEmpty())
        {
            QMessageBox::information(mMainWindow, tr("波段组合"),
                tr("需要至少3个波段进行RGB组合，当前图层只有%1个波段。").arg(bc));
            return;
        }

        QList<QPair<QString, QString>> bands = svc->productBands(img.productId);
        if (bands.size() < 3)
        {
            QMessageBox::information(mMainWindow, tr("波段组合"),
                tr("产品 %1 的可用波段不足3个。").arg(img.productId));
            return;
        }

        ProductBandDialog dlg(bands, mMainWindow);
        if (dlg.exec() == QDialog::Accepted)
        {
            svc->createRgbComposite(img.productId,
                                     dlg.redLayerId(), dlg.greenLayerId(), dlg.blueLayerId());
        }
    });

    // 导出图层
    connect(panel, &LayerPanel::exportLayerRequested, this, [this, svc](const QString& layerId)
    {
        RasterImage img = svc->layerImage(layerId);
        QString defaultName = img.displayName.isEmpty()
            ? QStringLiteral("exported") : img.displayName;
        defaultName.replace('/', '_').replace('\\', '_');
        QString path = QFileDialog::getSaveFileName(mMainWindow, tr("导出图层"),
            QDir::homePath() + "/" + defaultName + ".tif",
            tr("GeoTIFF (*.tif *.tiff);;所有文件 (*.*)"));
        if (path.isEmpty()) return;

        ExportDialog expDlg(mMainWindow);
        if (expDlg.exec() != QDialog::Accepted) return;

        QProgressDialog progress(tr("正在导出图层..."), QString(), 0, 0, mMainWindow);
        progress.setWindowModality(Qt::WindowModal);
        progress.setCancelButton(nullptr);
        progress.setMinimumDuration(500);
        progress.show();

        bool ok = false;
        QFuture<bool> future = QtConcurrent::run(
            svc, &ILayerService::exportLayer, layerId, path, expDlg.options());
        while (!future.isFinished())
            QApplication::processEvents();
        ok = future.result();

        progress.close();
        if (!ok)
            QMessageBox::warning(mMainWindow, tr("导出失败"),
                tr("无法导出图层: %1").arg(img.displayName));
    });

    // 矢量图层样式设置
    connect(panel, &LayerPanel::vectorStyleRequested, this, [this, svc](const QString& layerId)
    {
        VectorLayerInfo vInfo = svc->vectorLayerInfo(layerId);
        if (vInfo.layerId.isEmpty()) return;

        VectorStyleDialog dlg(mMainWindow);
        dlg.setFieldNames(vInfo.fieldNames);

        // Read current layer style to pre-populate the dialog
        VectorStyleConfig cfg;
        cfg.classifyField = vInfo.fieldNames.isEmpty() ? QString() : vInfo.fieldNames.first();

        QgsMapLayer* ml = svc->mapLayer(layerId);
        QgsVectorLayer* vl = qobject_cast<QgsVectorLayer*>(ml);
        if (vl)
        {
            QgsSingleSymbolRenderer* renderer =
                dynamic_cast<QgsSingleSymbolRenderer*>(vl->renderer());
            if (renderer && renderer->symbol())
            {
                QgsFillSymbol* fill = dynamic_cast<QgsFillSymbol*>(renderer->symbol());
                if (fill)
                {
                    QColor fc = fill->color();
                    if (fc.isValid()) cfg.fillColor = fc;
                    QgsSimpleLineSymbolLayer* sl =
                        dynamic_cast<QgsSimpleLineSymbolLayer*>(fill->symbolLayer(1));
                    if (sl)
                    {
                        QColor sc = sl->color();
                        if (sc.isValid()) cfg.strokeColor = sc;
                        cfg.strokeWidth = sl->width();
                    }
                }
                QgsLineSymbol* line = dynamic_cast<QgsLineSymbol*>(renderer->symbol());
                if (line)
                {
                    QColor lc = line->color();
                    if (lc.isValid()) cfg.strokeColor = lc;
                    cfg.strokeWidth = line->width();
                }
                QgsMarkerSymbol* marker = dynamic_cast<QgsMarkerSymbol*>(renderer->symbol());
                if (marker)
                {
                    QColor mc = marker->color();
                    if (mc.isValid()) { cfg.fillColor = mc; cfg.strokeColor = mc; }
                    cfg.markerSize = marker->size();
                }
            }
        }

        dlg.setConfig(cfg);

        if (dlg.exec() == QDialog::Accepted)
        {
            svc->setVectorStyle(layerId, dlg.config());
            // Force immediate processing of the scheduled canvas repaint
            QApplication::processEvents();
        }
    });

    // 图层透明度
    connect(panel, &LayerPanel::opacityChanged, this, [svc](const QString& layerId, double val)
    {
        svc->setLayerOpacity(layerId, val);
    });

    // 选中图层时同步透明度滑块
    connect(panel, &LayerPanel::layerSelectionChanged, this, [svc, panel](const QString& layerId)
    {
        if (!layerId.isEmpty())
            panel->syncOpacity(svc->layerOpacity(layerId));
    });

    // 选中图层时更新元数据面板（栅格或矢量）
    auto* metaPanel = mMainWindow->metadataPanel();
    auto* vecMetaPanel = mMainWindow->vectorMetadataPanel();
    if (metaPanel && vecMetaPanel)
    {
        connect(panel, &LayerPanel::layerSelectionChanged, this,
            [this, svc, metaPanel, vecMetaPanel](const QString& layerId)
            {
                if (layerId.isEmpty())
                {
                    metaPanel->clear();
                    vecMetaPanel->clear();
                    return;
                }

                // Try raster first
                RasterImage img = svc->layerImage(layerId);
                if (!img.rasterSize.isEmpty())
                {
                    mMainWindow->showRasterMetadata();

                    QString datum;
                    if (!img.projectionWkt.isEmpty())
                    {
                        QByteArray wkt = img.projectionWkt.toUtf8();
                        char* pszWkt = wkt.data();
                        OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);
                        if (OSRImportFromWkt(hSRS, &pszWkt) == OGRERR_NONE)
                        {
                            const char* d = OSRGetAttrValue(hSRS, "DATUM", 0);
                            if (d) datum = QString::fromUtf8(d);
                        }
                        OSRDestroySpatialReference(hSRS);
                    }

                    QString latLonDms, latLonDecimal;
                    LatLonBounds ll = GeoTransformUtils::computeLatLonBounds(
                        img.geoTransform, img.rasterSize, img.projectionWkt);
                    if (ll.valid)
                    {
                        latLonDms = QStringLiteral("经度: %1 ~ %2\n纬度: %3 ~ %4")
                            .arg(GeoTransformUtils::formatDms(ll.minLon, false),
                                 GeoTransformUtils::formatDms(ll.maxLon, false),
                                 GeoTransformUtils::formatDms(ll.minLat, true),
                                 GeoTransformUtils::formatDms(ll.maxLat, true));
                        latLonDecimal = QStringLiteral("(%1°, %2°) ~ (%3°, %4°)")
                            .arg(ll.minLon, 0, 'f', 6)
                            .arg(ll.minLat, 0, 'f', 6)
                            .arg(ll.maxLon, 0, 'f', 6)
                            .arg(ll.maxLat, 0, 'f', 6);
                    }

                    metaPanel->showMetadata(
                        layerId, img.displayName,
                        img.rasterSize.width(), img.rasterSize.height(),
                        img.bandCount,
                        img.projectionWkt, img.epsgCode,
                        img.geoTransform.size() >= 6 ? std::abs(img.geoTransform[1]) : 0.0,
                        img.geoTransform.size() >= 6 ? std::abs(img.geoTransform[5]) : 0.0,
                        datum, img.noDataValue,
                        img.dataType, img.rasterSourcePath,
                        latLonDms, latLonDecimal);
                    return;
                }

                // Try vector
                VectorLayerInfo vInfo = svc->vectorLayerInfo(layerId);
                if (!vInfo.layerId.isEmpty())
                {
                    mMainWindow->showVectorMetadata();

                    QString datum;
                    if (!vInfo.projectionWkt.isEmpty())
                    {
                        QByteArray wkt = vInfo.projectionWkt.toUtf8();
                        char* pszWkt = wkt.data();
                        OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);
                        if (OSRImportFromWkt(hSRS, &pszWkt) == OGRERR_NONE)
                        {
                            const char* d = OSRGetAttrValue(hSRS, "DATUM", 0);
                            if (d) datum = QString::fromUtf8(d);
                        }
                        OSRDestroySpatialReference(hSRS);
                    }
                    vecMetaPanel->showMetadata(vInfo, datum);
                    return;
                }

                metaPanel->clear();
                vecMetaPanel->clear();
            });
    }
}

void ApplicationController::wireMapSignals()
{
    auto* canvas = mMainWindow->mapCanvasWidget();
    if (!canvas) return;

    // 将 QGIS 画布注入图层服务
    static_cast<LayerServiceImpl*>(mLayerSvc.get())->setMapCanvas(canvas->mapCanvas());

    auto* statusBar = mMainWindow->statusBar();

    // 视口范围变更 → 状态栏显示当前范围
    connect(canvas, &MapCanvasWidget::canvasExtentChanged, this, [statusBar](const QgsRectangle& extent)
    {
        if (statusBar)
        {
            statusBar->showMessage(QStringLiteral(" 范围: [%1, %2] → [%3, %4]  |  %5 × %6")
                .arg(extent.xMinimum(), 0, 'f', 3)
                .arg(extent.yMinimum(), 0, 'f', 3)
                .arg(extent.xMaximum(), 0, 'f', 3)
                .arg(extent.yMaximum(), 0, 'f', 3)
                .arg(extent.width(),  0, 'f', 1)
                .arg(extent.height(), 0, 'f', 1),
                5000);
        }
    });

    // 左键点击地图 → 状态栏显示坐标
    connect(canvas, &MapCanvasWidget::mapClicked, this, [statusBar](const QgsPointXY& point)
    {
        if (statusBar)
        {
            statusBar->showMessage(QStringLiteral(" 坐标: %1, %2")
                .arg(point.x(), 0, 'f', 6)
                .arg(point.y(), 0, 'f', 6),
                10000);
        }
    });

    // 右键点击 → 状态栏显示坐标
    connect(canvas, &MapCanvasWidget::mapRightClicked, this, [statusBar](const QgsPointXY& point)
    {
        if (statusBar)
        {
            statusBar->showMessage(QStringLiteral(" 右键坐标: %1, %2")
                .arg(point.x(), 0, 'f', 6)
                .arg(point.y(), 0, 'f', 6),
                3000);
        }
    });

    // Spectral pick: extract pixel spectrum and show in dialog
    connect(canvas, &MapCanvasWidget::spectralPickRequested, this,
        [this, canvas](const QgsPointXY& geoPoint)
    {
        auto* dlg = mMainWindow->spectralDialog();
        if (!dlg) return;

        QString layerId = mMainWindow->layerPanel()->currentLayerId();
        if (layerId.isEmpty()) return;

        RasterImage img = mLayerSvc->layerImage(layerId);
        if (img.rasterSize.isEmpty() || img.geoTransform.size() < 6) return;

        auto pixel = GeoTransformUtils::geoToPixel(img.geoTransform,
            geoPoint.x(), geoPoint.y());
        int col = (int)pixel.first;
        int row = (int)pixel.second;

        if (col < 0 || col >= img.rasterSize.width() ||
            row < 0 || row >= img.rasterSize.height()) return;

        GdalRasterReader reader;
        if (!reader.open(img.rasterSourcePath.isEmpty() ? img.filePath : img.rasterSourcePath))
            return;

        int nBands = reader.bandCount();
        SpectralData sd;
        sd.layerName = img.displayName.isEmpty() ? layerId : img.displayName;
        sd.pixelCol  = col;
        sd.pixelRow  = row;
        sd.geoX      = geoPoint.x();
        sd.geoY      = geoPoint.y();
        sd.bandValues.resize(nBands);

        for (int b = 0; b < nBands; ++b)
        {
            QVector<float> vals = reader.readBandWindow(b + 1, col, row, 1, 1);
            sd.bandValues[b] = vals.isEmpty() ? NAN : vals[0];
        }
        reader.close();

        // Place marker on canvas at clicked point
        auto* marker = new QgsVertexMarker(canvas->mapCanvas());
        marker->setCenter(geoPoint);
        marker->setColor(Qt::red);
        marker->setIconSize(12);
        marker->setIconType(QgsVertexMarker::ICON_CROSS);
        marker->setPenWidth(2);

        // Remove previous marker if exists
        static QgsVertexMarker* sPrevMarker = nullptr;
        if (sPrevMarker)
        {
            canvas->mapCanvas()->scene()->removeItem(sPrevMarker);
            delete sPrevMarker;
        }
        sPrevMarker = marker;

        dlg->addProfile(sd);
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    });

}

void ApplicationController::wireRadiometricSignals()
{
    auto* svc = mRadiometricSvc.get();

    // MainWindow → Service: 执行辐射定标
    connect(mMainWindow, &MainWindow::calibrationRequested,
            svc, &IRadiometricService::execute);

    // Service → MainWindow 状态栏: 进度 / 完成 / 错误
    auto* statusBar = mMainWindow->statusBar();
    connect(svc, &IRadiometricService::progressChanged, this,
        [statusBar](int percent, const QString& step)
        {
            if (statusBar)
                statusBar->showMessage(QStringLiteral("辐射定标: %1% - %2").arg(percent).arg(step), 5000);
        });
    connect(svc, &IRadiometricService::finished, this,
        [this](bool success, const QString& outputPath)
        {
            if (mPipelineRunning) return;  // 流程模式不弹窗
            if (success)
                QMessageBox::information(mMainWindow, tr("辐射定标完成"),
                    tr("处理成功！\n\n输出路径:\n%1").arg(outputPath));
            else
                QMessageBox::warning(mMainWindow, tr("辐射定标失败"),
                    tr("处理未成功完成，请检查参数或日志。"));
        });
    connect(svc, &IRadiometricService::errorOccurred, this,
        [this](const QString& error)
        {
            QMessageBox::critical(mMainWindow, tr("辐射定标错误"), error);
        });
}

void ApplicationController::wireFusionSignals()
{
    auto* svc = mFusionSvc.get();
    auto* statusBar = mMainWindow->statusBar();

    // MainWindow → Service
    connect(mMainWindow, &MainWindow::fusionRequested,
            svc, &IFusionService::execute);

    // Service → UI
    connect(svc, &IFusionService::progressChanged, this,
        [statusBar](int percent, const QString& step)
        {
            if (statusBar)
                statusBar->showMessage(QStringLiteral("图像融合: %1% - %2").arg(percent).arg(step), 5000);
        });
    connect(svc, &IFusionService::finished, this,
        [this](bool success, const QString& outputPath)
        {
            if (mPipelineRunning) return;
            if (success)
                QMessageBox::information(mMainWindow, tr("融合完成"),
                    tr("融合成功！\n\n输出路径:\n%1").arg(outputPath));
            else
                QMessageBox::warning(mMainWindow, tr("融合失败"),
                    tr("融合未成功完成，请检查输入影像和参数。"));
        });
    connect(svc, &IFusionService::errorOccurred, this,
        [this](const QString& error)
        {
            QMessageBox::critical(mMainWindow, tr("融合错误"), error);
        });
    connect(svc, &IFusionService::qualityMetricsReady, this,
        [this](const FusionQualityMetrics& m)
        {
            QMessageBox::information(mMainWindow, tr("质量评价"),
                tr("相关系数: %1\n平均梯度: %2\nRMSE: %3\nSSIM: %4")
                    .arg(m.correlationCoefficient, 0, 'f', 4)
                    .arg(m.averageGradient, 0, 'f', 2)
                    .arg(m.rmse, 0, 'f', 4)
                    .arg(m.ssim, 0, 'f', 4));
        });
}

void ApplicationController::wireMosaicSignals()
{
    auto* svc = mMosaicSvc.get();
    auto* statusBar = mMainWindow->statusBar();

    connect(mMainWindow, &MainWindow::mosaicRequested,
            svc, &IMosaicService::execute);

    connect(svc, &IMosaicService::progressChanged, this,
        [statusBar](int percent, const QString& step)
        {
            if (statusBar)
                statusBar->showMessage(
                    QStringLiteral("镶嵌成图: %1% - %2").arg(percent).arg(step), 5000);
        });
    connect(svc, &IMosaicService::finished, this,
        [this](bool success, const QString& outputPath)
        {
            if (mPipelineRunning) return;
            if (success)
                QMessageBox::information(mMainWindow, tr("镶嵌完成"),
                    tr("镶嵌成功！\n\n输出路径:\n%1").arg(outputPath));
            else
                QMessageBox::warning(mMainWindow, tr("镶嵌失败"),
                    tr("镶嵌未成功完成，请检查输入影像和参数。"));
        });
    connect(svc, &IMosaicService::errorOccurred, this,
        [this](const QString& error)
        {
            QMessageBox::critical(mMainWindow, tr("镶嵌错误"), error);
        });
}

void ApplicationController::wireGeometricSignals()
{
    auto* svc = mGeometricSvc.get();
    auto* statusBar = mMainWindow->statusBar();

    // MainWindow -> Service: convert GeometricInput to GeometricCorrectionParams and execute
    connect(mMainWindow, &MainWindow::correctionRequested, this,
        [svc](const GeometricInput& input) {
            GeometricCorrectionParams params;
            params.sourceImage        = input.sourceImage;
            params.referenceImage     = input.referenceImage;
            params.outputPath         = input.outputPath;
            params.matchingMode       = input.matchingMode;
            params.matching.method    = input.matchingAlgorithm;
            params.matching.ratioThreshold  = input.ratioThreshold;
            params.matching.ransacThreshold = input.ransacThreshold;
            params.matching.maxFeatures     = input.maxFeatures;
            params.modelType          = input.modelType;
            params.resampleMethod     = input.resampleMethod;
            params.outputProjection   = input.outputProjection;
            params.outputPixelSizeX   = input.outputPixelSizeX;
            params.outputPixelSizeY   = input.outputPixelSizeY;
            params.outputExtent[0]    = input.outputExtent[0];
            params.outputExtent[1]    = input.outputExtent[1];
            params.outputExtent[2]    = input.outputExtent[2];
            params.outputExtent[3]    = input.outputExtent[3];
            params.blockSize          = input.blockSize;
            // Convert GcpEntry to Gcp
            for (const auto& e : input.gcps) {
                Gcp g;
                g.srcX = e.srcX; g.srcY = e.srcY;
                g.refX = e.refX; g.refY = e.refY;
                g.residual = e.residual;
                params.gcps.append(g);
            }
            svc->execute(params);
        });

    // Service -> UI: progress / finished / error
    connect(svc, &IGeometricService::progressChanged, this,
        [statusBar](int percent, const QString& step) {
            if (statusBar)
                statusBar->showMessage(QString("Geometric Correction: %1% - %2").arg(percent).arg(step), 5000);
        });
    connect(svc, &IGeometricService::finished, this,
        [this](bool success, const QString& outputPath) {
            if (mPipelineRunning) return;
            if (success)
                QMessageBox::information(mMainWindow, tr("Geometric Correction Complete"),
                    tr("Processing succeeded!\n\nOutput:\n%1").arg(outputPath));
            else
                QMessageBox::warning(mMainWindow, tr("Geometric Correction Failed"),
                    tr("Processing did not complete successfully."));
        });
    connect(svc, &IGeometricService::errorOccurred, this,
        [this](const QString& error) {
            QMessageBox::critical(mMainWindow, tr("Geometric Correction Error"), error);
        });

    // Cancel
    connect(mMainWindow, &MainWindow::cancelCorrectionRequested, this, [svc]() {
        svc->cancel();
    });
}

void ApplicationController::wireWorkflowSignals()
{
    auto* svc = mWorkflowSvc.get();

    // 确保 PipelineDialog 已创建并完成信号连接
    auto ensureDialog = [this, svc]() -> PipelineDialog*
    {
        if (!mPipelineDialog)
        {
            mPipelineDialog = new PipelineDialog(mMainWindow);
            mPipelineDialog->setAttribute(Qt::WA_DeleteOnClose, false);

            connect(mPipelineDialog, &PipelineDialog::runRequested, this,
                [this, svc](const Project& p) {
                    mPipelineRunning = p.output.autoConfirm;
                    svc->runProject(p);
                    mPipelineRunning = false;
                });

            connect(mPipelineDialog, &PipelineDialog::saveRequested, this,
                [this](const QString& fp) {
                    mProjectRepo->save(mPipelineDialog->project(), fp);
                });

            connect(mPipelineDialog, &PipelineDialog::loadRequested, this,
                [this](const QString& fp) {
                    Project p = mProjectRepo->load(fp);
                    if (p.isValid()) mPipelineDialog->setProject(p);
                });

            connect(svc, &IWorkflowService::nodeProgressChanged,
                mPipelineDialog, &PipelineDialog::onStageProgress);
            connect(svc, &IWorkflowService::workflowFinished,
                mPipelineDialog, &PipelineDialog::onPipelineFinished);
            connect(svc, &IWorkflowService::nodeError,
                mPipelineDialog, &PipelineDialog::onPipelineError);
        }
        return mPipelineDialog;
    };

    // Ribbon "新建工程" → 打开 PipelineDialog（默认标准流程）
    connect(mMainWindow, &MainWindow::workflowNewRequested, this, [this, ensureDialog]()
    {
        Project p;
        p.pipeline = PipelineExpander::standardPipeline();
        auto* dlg = ensureDialog();
        dlg->setProject(p);
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    });

    // Ribbon "保存工程" → 直接保存
    connect(mMainWindow, &MainWindow::workflowSaveRequested, this,
        [this]()
        {
            if (!mPipelineDialog) return;
            QString file = QFileDialog::getSaveFileName(mMainWindow,
                QStringLiteral("保存工程"), QString(),
                QStringLiteral("遥感工程文件 (*.rjp);;所有文件 (*.*)"));
            if (!file.isEmpty())
                mProjectRepo->save(mPipelineDialog->project(), file);
        });

    // Ribbon "加载工程" → 加载并打开对话框
    connect(mMainWindow, &MainWindow::workflowLoadRequested, this,
        [this, ensureDialog]()
        {
            QString file = QFileDialog::getOpenFileName(mMainWindow,
                QStringLiteral("加载工程"), QString(),
                QStringLiteral("遥感工程文件 (*.rjp);;所有文件 (*.*)"));
            if (file.isEmpty()) return;

            Project proj = mProjectRepo->load(file);
            if (!proj.isValid())
            {
                QMessageBox::warning(mMainWindow, QStringLiteral("加载失败"),
                    QStringLiteral("无法解析工程文件: %1").arg(file));
                return;
            }

            auto* dlg = ensureDialog();
            dlg->setProject(proj);
            dlg->show();
            dlg->raise();
            dlg->activateWindow();
        });
}

void ApplicationController::wireToolBoxSignals()
{
    auto* toolBox = mMainWindow->toolBoxPanel();
    if (!toolBox) return;

    auto* statusBar = mMainWindow->statusBar();

    connect(toolBox, &ToolBoxPanel::toolRequested, this,
        [this, statusBar](const QString& toolId)
    {
        if (toolId == QStringLiteral("raster_clip"))
        {
            RasterClipDialog dlg(mMainWindow);
            if (dlg.exec() == QDialog::Accepted)
                mRasterClipSvc->execute(dlg.params());
        }
        else if (toolId == QStringLiteral("raster_reproject"))
        {
            RasterReprojectionDialog dlg(mMainWindow);
            if (dlg.exec() == QDialog::Accepted)
                mRasterReprojSvc->execute(dlg.params());
        }
        else if (toolId == QStringLiteral("vector_reproject"))
        {
            VectorReprojectionDialog dlg(mMainWindow);
            if (dlg.exec() == QDialog::Accepted)
                mVectorReprojSvc->execute(dlg.params());
        }
        else if (toolId == QStringLiteral("define_projection"))
        {
            DefineProjectionDialog dlg(mMainWindow);
            if (dlg.exec() == QDialog::Accepted)
                mDefineProjSvc->execute(dlg.params());
        }
    });

    // Progress → status bar
    connect(mRasterClipSvc.get(), &IRasterClipService::progressChanged, this,
        [statusBar](int percent, const QString& step) {
            if (statusBar)
                statusBar->showMessage(
                    QString::fromUtf8("\xe6\xa0\x85\xe6\xa0\xbc\xe8\xa3\x81\xe5\x89\xaa: %1% - %2")
                        .arg(percent).arg(step), 5000);
        });

    // Finished → auto-load result + result panel
    connect(mRasterClipSvc.get(), &IRasterClipService::finished, this,
        [this, toolBox](bool success, const QString& outputPath) {
            if (mPipelineRunning) return;
            if (success) {
                // Auto-load the clipped result onto the canvas
                mLayerSvc->addLayers({outputPath});
                toolBox->appendResult(true,
                    QString::fromUtf8("\xe6\xa0\x85\xe6\xa0\xbc\xe8\xa3\x81\xe5\x89\xaa"),
                    QString::fromUtf8("\xe5\xae\x8c\xe6\x88\x90"));
            } else {
                QMessageBox::warning(mMainWindow,
                    QString::fromUtf8("\xe8\xa3\x81\xe5\x89\xaa\xe5\xa4\xb1\xe8\xb4\xa5"),
                    QString::fromUtf8("\xe8\xa3\x81\xe5\x89\xaa\xe6\x9c\xaa\xe6\x88\x90\xe5\x8a\x9f\xe5\xae\x8c\xe6\x88\x90\xef\xbc\x8c\xe8\xaf\xb7\xe6\xa3\x80\xe6\x9f\xa5\xe8\xbe\x93\xe5\x85\xa5\xe6\x96\x87\xe4\xbb\xb6\xe5\x92\x8c\xe5\x8f\x82\xe6\x95\xb0\xe3\x80\x82"));
                toolBox->appendResult(false,
                    QString::fromUtf8("\xe6\xa0\x85\xe6\xa0\xbc\xe8\xa3\x81\xe5\x89\xaa"),
                    QString::fromUtf8("\xe5\xa4\xb1\xe8\xb4\xa5"));
            }
        });

    connect(mRasterClipSvc.get(), &IRasterClipService::errorOccurred, this,
        [this](const QString& error) {
            QMessageBox::critical(mMainWindow,
                QString::fromUtf8("\xe8\xa3\x81\xe5\x89\xaa\xe9\x94\x99\xe8\xaf\xaf"), error);
        });

    // ── Raster reprojection wiring ──
    connect(mRasterReprojSvc.get(), &IRasterReprojectionService::progressChanged, this,
        [statusBar](int p, const QString& s) {
            if (statusBar) statusBar->showMessage(QStringLiteral("RasterReproj: %1% - %2").arg(p).arg(s), 5000);
        });
    connect(mRasterReprojSvc.get(), &IRasterReprojectionService::finished, this,
        [this, toolBox](bool ok, const QString& path) {
            if (mPipelineRunning) return;
            if (ok) { mLayerSvc->addLayers({path}); toolBox->appendResult(true, QString::fromUtf8("\xe6\xa0\x85\xe6\xa0\xbc\xe9\x87\x8d\xe6\x8a\x95\xe5\xbd\xb1"), QString::fromUtf8("\xe5\xae\x8c\xe6\x88\x90")); }
            else { QMessageBox::warning(mMainWindow, tr("Failed"), tr("Reprojection failed.")); toolBox->appendResult(false, QString::fromUtf8("\xe6\xa0\x85\xe6\xa0\xbc\xe9\x87\x8d\xe6\x8a\x95\xe5\xbd\xb1"), tr("Failed")); }
        });
    connect(mRasterReprojSvc.get(), &IRasterReprojectionService::errorOccurred, this,
        [this](const QString& e) { QMessageBox::critical(mMainWindow, tr("Error"), e); });

    // ── Vector reprojection wiring ──
    connect(mVectorReprojSvc.get(), &IVectorReprojectionService::progressChanged, this,
        [statusBar](int p, const QString& s) {
            if (statusBar) statusBar->showMessage(QStringLiteral("VectorReproj: %1% - %2").arg(p).arg(s), 5000);
        });
    connect(mVectorReprojSvc.get(), &IVectorReprojectionService::finished, this,
        [this, toolBox](bool ok, const QString& path) {
            if (mPipelineRunning) return;
            if (ok) { mLayerSvc->addVectorLayers({path}); toolBox->appendResult(true, QString::fromUtf8("\xe7\x9f\xa2\xe9\x87\x8f\xe9\x87\x8d\xe6\x8a\x95\xe5\xbd\xb1"), QString::fromUtf8("\xe5\xae\x8c\xe6\x88\x90")); }
            else { QMessageBox::warning(mMainWindow, tr("Failed"), tr("Reprojection failed.")); toolBox->appendResult(false, QString::fromUtf8("\xe7\x9f\xa2\xe9\x87\x8f\xe9\x87\x8d\xe6\x8a\x95\xe5\xbd\xb1"), tr("Failed")); }
        });
    connect(mVectorReprojSvc.get(), &IVectorReprojectionService::errorOccurred, this,
        [this](const QString& e) { QMessageBox::critical(mMainWindow, tr("Error"), e); });

    // ── Define projection wiring ──
    connect(mDefineProjSvc.get(), &IDefineProjectionService::progressChanged, this,
        [statusBar](int p, const QString& s) {
            if (statusBar) statusBar->showMessage(QStringLiteral("DefineProj: %1% - %2").arg(p).arg(s), 5000);
        });
    connect(mDefineProjSvc.get(), &IDefineProjectionService::finished, this,
        [this, toolBox](bool ok, const QString& path) {
            if (mPipelineRunning) return;
            if (ok) { toolBox->appendResult(true, QString::fromUtf8("\xe5\xae\x9a\xe4\xb9\x89\xe6\x8a\x95\xe5\xbd\xb1"), QString::fromUtf8("\xe5\xae\x8c\xe6\x88\x90: %1").arg(path)); }
            else { QMessageBox::warning(mMainWindow, tr("Failed"), tr("Define projection failed.")); toolBox->appendResult(false, QString::fromUtf8("\xe5\xae\x9a\xe4\xb9\x89\xe6\x8a\x95\xe5\xbd\xb1"), tr("Failed")); }
        });
    connect(mDefineProjSvc.get(), &IDefineProjectionService::errorOccurred, this,
        [this](const QString& e) { QMessageBox::critical(mMainWindow, tr("Error"), e); });
}

void ApplicationController::wireGeneralSignals()
{
    // 通用操作（撤销、重做、帮助等）由 MainWindow 直接处理
}
