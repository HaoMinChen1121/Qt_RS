#include "LayerServiceImpl.h"
#include "dataaccess/IRasterReader.h"
#include "dataaccess/IVectorReader.h"
#include "dataaccess/ISensorProduct.h"
#include "dataaccess/SensorProductFactory.h"
#include <qgsrasterlayer.h>
#include <qgsvectorlayer.h>
#include <qgssinglesymbolrenderer.h>
#include <qgsfillsymbol.h>
#include <qgslinesymbol.h>
#include <qgsmarkersymbol.h>
#include <qgscategorizedsymbolrenderer.h>
#include <qgsgraduatedsymbolrenderer.h>
#include <qgscolorramp.h>
#include <qgsclassificationmethod.h>
#include <qgsclassificationjenks.h>
#include <qgsclassificationequalinterval.h>
#include <qgsclassificationquantile.h>
#include <qgsclassificationstandarddeviation.h>
#include <qgsmapcanvas.h>
#include <qgsmaplayer.h>
#include <qgsrectangle.h>
#include <qgsmultibandcolorrenderer.h>
#include <qgssinglebandgrayrenderer.h>
#include <qgsrasterdataprovider.h>
#include <qgscontrastenhancement.h>
#include <qgsrasterbandstats.h>
#include <gdal_priv.h>
#include <ogr_srs_api.h>
#include <cpl_vsi.h>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>
#include <QThread>

LayerServiceImpl::LayerServiceImpl(IRasterReader* reader, IVectorReader* vecReader, QObject* parent)
    : ILayerService(), mReader(reader), mVectorReader(vecReader)
    {
    setParent(parent);
}

LayerServiceImpl::~LayerServiceImpl()
{
    if (mCanvas)
    {
        mCanvas->setLayers({});
        mCanvas->waitWhileRendering();
    }

    // Delete QGIS layer objects first — they hold GDAL dataset handles
    // that must be released before we can safely remove the VRT files.
    qDeleteAll(mMapLayers);
    mMapLayers.clear();
    qDeleteAll(mVectorMapLayers);
    mVectorMapLayers.clear();

    // Now safe to remove temp VRT files
    int removed = 0;
    for (const auto& img : mLayers)
    {
        if (img.rasterSourcePath.endsWith(QStringLiteral(".vrt"), Qt::CaseInsensitive)
            && img.rasterSourcePath.contains(QDir::tempPath()))
        {
            if (QFile::remove(img.rasterSourcePath))
            {
                qDebug() << "[LayerService] ~dtor removed temp VRT:" << img.rasterSourcePath;
                ++removed;
            }
        }
    }
    if (removed > 0)
        qDebug() << "[LayerService] ~dtor cleaned up" << removed << "temp VRT files";
}

void LayerServiceImpl::setMapCanvas(QgsMapCanvas* canvas)
{
    mCanvas = canvas;
    if (mCanvas)
        mCanvas->setCachingEnabled(true);
}

void LayerServiceImpl::rebuildCanvasLayers()
{
    if (!mCanvas)
        return;

    const QgsRectangle currentExtent = mCanvas->extent();
    const bool hasExtent = !currentExtent.isEmpty();

    // Tree index 0 (top) renders first → bottom of canvas;
    // tree last renders last → top of canvas.
    QList<QgsMapLayer*> visibleLayers;
    for (int i = 0; i < mLayerOrder.size(); ++i)
    {
        const QString& id = mLayerOrder[i];
        QgsMapLayer* ml = findMapLayer(id);
        if (!ml) continue;

        for (const auto& img : mLayers)
            if (img.layerId == id && img.visible) { visibleLayers.append(ml); break; }
        for (const auto& vInfo : mVectorLayerInfos)
            if (vInfo.layerId == id && vInfo.visible) { visibleLayers.append(ml); break; }
    }

    mCanvas->setLayers(visibleLayers);

    if (hasExtent)
        mCanvas->setExtent(currentExtent);
    else
        mCanvas->zoomToFullExtent();

    mCanvas->refresh();
}

// ─── 添加图层 ────────────────────────────────────────────────────────────────

QStringList LayerServiceImpl::addLayers(const QStringList& filePaths)
{
    qDebug() << "[LayerService] addLayers:" << filePaths;
    QStringList addedIds;

    for (const QString& path : filePaths)
    {
        QScopedPointer<ISensorProduct> product(createSensorProduct(path));

        if (product && product->open(path))
        {
            // ─── 按产品加载：枚举波段，逐个注册为图层 ───
            QList<RasterBandDescriptor> descs = product->bands();
            if (descs.isEmpty())
            {
                qWarning() << "[LayerService] product has no bands:" << path;
                emit layerError(path, QStringLiteral("产品中没有识别到波段文件"));
                product->close();
                continue;
            }

            for (const RasterBandDescriptor& desc : descs)
            {
                const QString id = QStringLiteral("layer_%1_%2_%3")
                    .arg(mLayers.size())
                    .arg(product->productId())
                    .arg(desc.bandName);

                // 通过 GDAL 读取单个波段元数据
                RasterImage img;
                img.layerId       = id;
                img.displayName   = QStringLiteral("%1 - %2").arg(product->productId(), desc.bandName);
                img.filePath      = path;                // 原始产品路径
                img.rasterSourcePath = desc.rasterPath;  // 实际 GDAL 路径
                img.productId     = product->productId();
                img.physicalBand  = desc.physicalBand;
                img.sensorType    = product->sensorType();
                img.visible       = true;

                if (mReader->open(desc.rasterPath))
                {
                    img = mReader->toRasterImage(id, img.displayName);
                    // 从 reader 恢复上面已设的字段 (toRasterImage 不会填充)
                    img.layerId         = id;
                    img.displayName     = img.displayName.isEmpty() ? img.displayName : img.displayName;
                    img.filePath        = path;
                    img.rasterSourcePath = desc.rasterPath;
                    img.productId       = product->productId();
                    img.physicalBand    = desc.physicalBand;
                    img.sensorType      = product->sensorType();
                    img.visible         = true;
                    mReader->close();
                } else
                {
                    img.bandCount  = 0;
                    img.rasterSize = desc.rasterSize;
                    img.dataType   = desc.dataType;
                    if (img.rasterSize.isEmpty())
                        img.rasterSize = QSize(10980, 10980); // S2 默认
                }

                // 创建 QGIS 栅格图层
                auto* rl = new QgsRasterLayer(desc.rasterPath, img.displayName, QStringLiteral("gdal"));
                if (!rl->isValid())
                {
                    qWarning() << "[LayerService] QGIS failed to load:" << desc.rasterPath;
                    emit layerError(id, QStringLiteral("QGIS 无法加载波段: ") + desc.bandName);
                    delete rl;
                    continue;
                }

                mLayers.prepend(img);
                mMapLayers.prepend(rl);
                mLayerOrder.prepend(id);
                addedIds.append(id);
                emit layerLoaded(id, img.displayName, img.dataType);
                qDebug() << "[LayerService] band added:" << id << desc.bandName
                         << "crs:" << rl->crs().authid();
            }

        }
        else if (!product)
        {
            // ─── 非产品模式：单个栅格文件直接加载 (原有逻辑) ───
            QFileInfo fi(path);
            const QString id = QStringLiteral("layer_%1_%2").arg(mLayers.size()).arg(fi.fileName());

            RasterImage img;
            img.layerId     = id;
            img.displayName = fi.fileName();
            img.filePath    = path;
            img.rasterSourcePath = path;
            img.visible     = true;

            if (mReader->open(path))
            {
                img = mReader->toRasterImage(id, fi.fileName());
                img.layerId         = id;
                img.displayName     = fi.fileName();
                img.filePath        = path;
                img.rasterSourcePath = path;
                img.visible         = true;
                mReader->close();
            } else
            {
                img.bandCount     = 0;
                img.rasterSize    = QSize();
                img.geoTransform  = {0, 1, 0, 0, 0, -1};
                img.epsgCode      = -1;
            }

            auto* rl = new QgsRasterLayer(path, fi.fileName(), QStringLiteral("gdal"));
            if (!rl->isValid())
            {
                qWarning() << "[LayerService] QGIS failed to load:" << path;
                emit layerError(id, QStringLiteral("QGIS 无法加载: ") + fi.fileName());
                delete rl;
                continue;
            }

            mLayers.prepend(img);
            mMapLayers.prepend(rl);
            mLayerOrder.prepend(id);
            addedIds.append(id);
            emit layerLoaded(id, fi.fileName(), img.dataType);
            qDebug() << "[LayerService] layer created:" << id << "crs:" << rl->crs().authid();
        }
        else
        {
            // product was recognized but failed to open
            qWarning() << "[LayerService] sensor product failed to open:" << path;
            emit layerError(path, QStringLiteral("传感器产品打开失败: ") + path);
        }
    }

    if (!addedIds.isEmpty())
    {
        rebuildCanvasLayers();
        emit renderLayersChanged(mLayers);
    }
    return addedIds;
}

// ─── 移除图层 ────────────────────────────────────────────────────────────────

void LayerServiceImpl::removeLayers(const QStringList& layerIds)
{
    qDebug() << "[LayerService] removeLayers:" << layerIds;
    bool changed = false;
    for (const QString& id : layerIds)
    {
        // Check raster layers
        const int idx = [&]() -> int {
            for (int i = 0; i < mLayers.size(); ++i)
                if (mLayers[i].layerId == id) return i;
            return -1;
        }();
        if (idx >= 0)
        {
            mLayerOrder.removeAll(id);
            const QString vrtPath = mLayers[idx].rasterSourcePath;
            mLayers.removeAt(idx);
            if (idx < mMapLayers.size()) {
                delete mMapLayers.takeAt(idx);
            }

            if (vrtPath.endsWith(QStringLiteral(".vrt"), Qt::CaseInsensitive)
                && vrtPath.contains(QDir::tempPath()))
            {
                if (QFile::remove(vrtPath))
                    qDebug() << "[LayerService] removeLayers deleted temp VRT:" << vrtPath;
            }
            emit layerRemoved(id);
            changed = true;
            continue;
        }

        // Check vector layers
        const int vIdx = vectorLayerIndex(id);
        if (vIdx >= 0)
        {
            mLayerOrder.removeAll(id);
            mVectorLayerInfos.removeAt(vIdx);
            if (vIdx < mVectorMapLayers.size()) {
                delete mVectorMapLayers.takeAt(vIdx);
            }
            emit layerRemoved(id);
            changed = true;
        }
    }
    if (changed)
    {
        rebuildCanvasLayers();
        emit renderLayersChanged(mLayers);
        emit vectorRenderLayersChanged(mVectorLayerInfos);
    }
}

// ─── 可见性 ──────────────────────────────────────────────────────────────────

void LayerServiceImpl::setLayerVisibility(const QString& layerId, bool visible)
{
    for (auto& img : mLayers)
    {
        if (img.layerId == layerId)
        {
            img.visible = visible;
            rebuildCanvasLayers();
            emit renderLayersChanged(mLayers);
            return;
        }
    }
    for (auto& vInfo : mVectorLayerInfos)
    {
        if (vInfo.layerId == layerId)
        {
            vInfo.visible = visible;
            rebuildCanvasLayers();
            emit vectorRenderLayersChanged(mVectorLayerInfos);
            return;
        }
    }
}

double LayerServiceImpl::layerOpacity(const QString& layerId) const
{
    for (const auto& img : mLayers)
        if (img.layerId == layerId)
            return img.opacity;
    return 1.0;
}

void LayerServiceImpl::setLayerOpacity(const QString& layerId, double opacity)
{
    // 持久化到模型 - 栅格
    for (auto& img : mLayers)
    {
        if (img.layerId == layerId)
        {
            img.opacity = opacity;
            QgsMapLayer* ml = findMapLayer(layerId);
            auto* rl = qobject_cast<QgsRasterLayer*>(ml);
            if (rl)
            {
                rl->setOpacity(opacity);
                rl->triggerRepaint();
                if (mCanvas)
                    mCanvas->refresh();
            }
            return;
        }
    }
    // 矢量
    for (auto& vInfo : mVectorLayerInfos)
    {
        if (vInfo.layerId == layerId)
        {
            vInfo.opacity = opacity;
            const int vIdx = vectorLayerIndex(layerId);
            if (vIdx >= 0 && vIdx < mVectorMapLayers.size() && mVectorMapLayers[vIdx])
            {
                mVectorMapLayers[vIdx]->setOpacity(opacity);
                if (mCanvas)
                    mCanvas->refresh();
            }
            return;
        }
    }
}

// ─── 排序 ────────────────────────────────────────────────────────────────────

void LayerServiceImpl::reorderLayers(const QStringList& orderedIds)
{
    // Build new unified order: ordered IDs first, then any remaining ones
    QStringList newOrder;
    QSet<QString> seen;
    for (const QString& id : orderedIds)
    {
        // Only include IDs that actually exist
        if (layerIndex(id) >= 0 || vectorLayerIndex(id) >= 0)
        {
            newOrder.append(id);
            seen.insert(id);
        }
    }
    // Append any IDs not in orderedIds (preserve their relative order from old list)
    for (const QString& id : mLayerOrder)
    {
        if (!seen.contains(id))
            newOrder.append(id);
    }

    mLayerOrder = newOrder;
    rebuildCanvasLayers();
    emit renderLayersChanged(mLayers);
    emit vectorRenderLayersChanged(mVectorLayerInfos);
}

// ─── 查询 ────────────────────────────────────────────────────────────────────

QRectF LayerServiceImpl::layerExtent(const QString& layerId) const
{
    // Check QGIS raster layers first
    for (int i = 0; i < mLayers.size(); ++i)
    {
        if (mLayers[i].layerId == layerId && i < mMapLayers.size() && mMapLayers[i])
        {
            const QgsRectangle ext = mMapLayers[i]->extent();
            return QRectF(ext.xMinimum(), ext.yMinimum(), ext.width(), ext.height());
        }
    }
    // Check QGIS vector layers
    for (int i = 0; i < mVectorLayerInfos.size(); ++i)
    {
        if (mVectorLayerInfos[i].layerId == layerId
            && i < mVectorMapLayers.size() && mVectorMapLayers[i])
        {
            const QgsRectangle ext = mVectorMapLayers[i]->extent();
            return QRectF(ext.xMinimum(), ext.yMinimum(), ext.width(), ext.height());
        }
    }
    // Fallback to stored extent
    for (const auto& img : mLayers)
    {
        if (img.layerId == layerId)
            return img.extent();
    }
    for (const auto& vInfo : mVectorLayerInfos)
    {
        if (vInfo.layerId == layerId)
            return vInfo.extent;
    }
    return {};
}

RasterImage LayerServiceImpl::layerImage(const QString& layerId) const
{
    for (const auto& img : mLayers)
    {
        if (img.layerId == layerId)
            return img;
    }
    return {};
}

// ─── 波段操作 ────────────────────────────────────────────────────────────────

QgsMapLayer* LayerServiceImpl::findMapLayer(const QString& layerId) const
{
    int idx = layerIndex(layerId);
    if (idx >= 0 && idx < mMapLayers.size())
        return mMapLayers[idx];
    int vIdx = vectorLayerIndex(layerId);
    if (vIdx >= 0 && vIdx < mVectorMapLayers.size())
        return mVectorMapLayers[vIdx];
    return nullptr;
}

int LayerServiceImpl::layerIndex(const QString& layerId) const
{
    for (int i = 0; i < mLayers.size(); ++i)
        if (mLayers[i].layerId == layerId) return i;
    return -1;
}

int LayerServiceImpl::bandCount(const QString& layerId) const
{
    QgsMapLayer* ml = findMapLayer(layerId);
    auto* rl = qobject_cast<QgsRasterLayer*>(ml);
    return rl ? rl->bandCount() : 0;
}

QString LayerServiceImpl::sensorTypeForLayer(const QString& layerId) const
{
    for (const auto& img : mLayers)
        if (img.layerId == layerId) return img.sensorType;
    return {};
}

void LayerServiceImpl::setBandRenderer(const QString& layerId,
                                         int red, int green, int blue)
                                         {
    QgsMapLayer* ml = findMapLayer(layerId);
    auto* rl = qobject_cast<QgsRasterLayer*>(ml);
    if (!rl || !rl->dataProvider()) return;

    auto* renderer = new QgsMultiBandColorRenderer(rl->dataProvider(), red, green, blue);
    rl->setRenderer(renderer);
    if (mCanvas) mCanvas->refresh();
}

QStringList LayerServiceImpl::splitToBands(const QString& layerId)
{
    QStringList newIds;
    QgsMapLayer* ml = findMapLayer(layerId);
    auto* rl = qobject_cast<QgsRasterLayer*>(ml);
    if (!rl) return newIds;

    int nBands = rl->bandCount();
    RasterImage srcImg = layerImage(layerId);

    for (int b = 1; b <= nBands; ++b)
    {
        auto* bandLayer = new QgsRasterLayer(srcImg.rasterSourcePath,
            QStringLiteral("%1 - B%2").arg(srcImg.displayName).arg(b),
            QStringLiteral("gdal"));
        if (!bandLayer->isValid()) { delete bandLayer; continue; }

        auto* gray = new QgsSingleBandGrayRenderer(bandLayer->dataProvider(), b);

        // 对比度增强: 2-sigma 裁剪抑制离群噪声, 消除高亮碎块
        {
            QgsRasterDataProvider* prov = bandLayer->dataProvider();
            QgsRasterBandStats stats = prov->bandStatistics(
                b,
                QgsRasterBandStats::Min | QgsRasterBandStats::Max
                | QgsRasterBandStats::Mean | QgsRasterBandStats::StdDev,
                QgsRectangle(), 0);

            auto* ce = new QgsContrastEnhancement(prov->dataType(b));
            ce->setContrastEnhancementAlgorithm(
                QgsContrastEnhancement::StretchToMinimumMaximum);

            if (!std::isnan(stats.mean) && !std::isnan(stats.stdDev)
                && stats.stdDev > 0.0)
            {
                double clipMin = std::max(stats.minimumValue,
                    stats.mean - 2.0 * stats.stdDev);
                double clipMax = std::min(stats.maximumValue,
                    stats.mean + 2.0 * stats.stdDev);
                ce->setMinimumValue(clipMin);
                ce->setMaximumValue(clipMax);
            }
            else
            {
                ce->setMinimumValue(stats.minimumValue);
                ce->setMaximumValue(stats.maximumValue);
            }
            gray->setContrastEnhancement(ce);
        }

        bandLayer->setRenderer(gray);

        const QString newId = QStringLiteral("layer_%1_%2_b%3")
            .arg(mLayers.size()).arg(srcImg.productId).arg(b);
        RasterImage img;
        img.layerId          = newId;
        img.displayName      = QStringLiteral("%1 - Band %2").arg(srcImg.displayName).arg(b);
        img.filePath         = srcImg.filePath;
        img.rasterSourcePath = srcImg.rasterSourcePath;
        img.productId        = srcImg.productId;
        img.physicalBand     = b;
        img.sensorType       = srcImg.sensorType;
        img.bandCount        = 1;
        img.visible          = true;

        mLayers.prepend(img);
        mMapLayers.prepend(bandLayer);
        mLayerOrder.prepend(newId);
        newIds.append(newId);
        emit layerLoaded(newId, img.displayName, QStringLiteral("Band"));
    }

    if (!newIds.isEmpty())
    {
        rebuildCanvasLayers();
        emit renderLayersChanged(mLayers);
    }
    return newIds;
}

// ─── 产品级波段操作 ──────────────────────────────────────────────────────────

QList<QPair<QString, QString>> LayerServiceImpl::productBands(const QString& productId) const
{
    QList<QPair<QString, QString>> result;
    if (productId.isEmpty()) return result;
    for (const auto& img : mLayers)
    {
        if (img.productId == productId && img.physicalBand > 0)
            result.append({img.layerId, img.displayName});
    }
    return result;
}

QString LayerServiceImpl::createRgbComposite(const QString& productId,
                                               const QString& redLayerId,
                                               const QString& greenLayerId,
                                               const QString& blueLayerId)
                                               {
    RasterImage rImg = layerImage(redLayerId);
    RasterImage gImg = layerImage(greenLayerId);
    RasterImage bImg = layerImage(blueLayerId);
    if (rImg.rasterSourcePath.isEmpty() || gImg.rasterSourcePath.isEmpty()
        || bImg.rasterSourcePath.isEmpty())
        return {};

    // ── 用 GDAL 探针 Red 源文件获取空间参考 + 实际尺寸 + 数据类型 ──
    GDALAllRegister();
    GDALDataset* probeDS = (GDALDataset*)GDALOpen(
        rImg.rasterSourcePath.toUtf8().constData(), GA_ReadOnly);
    if (!probeDS)
    {
        qWarning() << "[LayerService] GDAL cannot open source for VRT:"
                   << rImg.rasterSourcePath;
        return {};
    }

    int    srcW   = probeDS->GetRasterXSize();
    int    srcH   = probeDS->GetRasterYSize();
    double geo[6] = {};
    probeDS->GetGeoTransform(geo);
    const char* projWkt = probeDS->GetProjectionRef();
    QString projStr = QString::fromUtf8(projWkt);
    // SRS → WKT 或空
    QString srsXml;
    if (projStr.isEmpty())
    {
        // 回退到 EPSG 码
        OGRSpatialReference srs(probeDS->GetProjectionRef());
        srs.AutoIdentifyEPSG();
        if (srs.GetAuthorityCode("PROJCS") || srs.GetAuthorityCode("GEOGCS"))
            srsXml = QStringLiteral("<SRS>EPSG:%1</SRS>")
                .arg(QString::fromUtf8(srs.GetAuthorityCode("PROJCS")
                    ? srs.GetAuthorityCode("PROJCS")
                    : srs.GetAuthorityCode("GEOGCS")));
    }
    else
    {
        srsXml = QStringLiteral("<SRS>%1</SRS>").arg(projStr);
    }

    // 数据类型
    QString dataType = QString::fromUtf8(
        GDALGetDataTypeName(probeDS->GetRasterBand(1)->GetRasterDataType()));
    if (dataType.isEmpty()) dataType = QStringLiteral("UInt16");

    // 验证 3 波段尺寸一致
    {
        GDALDataset* gDS = (GDALDataset*)GDALOpen(
            gImg.rasterSourcePath.toUtf8().constData(), GA_ReadOnly);
        GDALDataset* bDS = (GDALDataset*)GDALOpen(
            bImg.rasterSourcePath.toUtf8().constData(), GA_ReadOnly);
        if (!gDS || !bDS || gDS->GetRasterXSize() != srcW
            || gDS->GetRasterYSize() != srcH
            || bDS->GetRasterXSize() != srcW
            || bDS->GetRasterYSize() != srcH)
            {
            qWarning() << "[LayerService] VRT band size mismatch, refusing";
            GDALClose(probeDS);
            if (gDS) GDALClose(gDS);
            if (bDS) GDALClose(bDS);
            return {};
        }
        GDALClose(gDS);
        GDALClose(bDS);
    }
    GDALClose(probeDS);

    // ── 构建完整 VRT XML (含空间参考) ──
    QString geoXml;
    if (geo[1] != 0.0 || geo[5] != 0.0)
        geoXml = QStringLiteral(
            "  <GeoTransform>%1, %2, %3, %4, %5, %6</GeoTransform>\n")
            .arg(geo[0], 0, 'f', 12).arg(geo[1], 0, 'f', 12)
            .arg(geo[2], 0, 'f', 12).arg(geo[3], 0, 'f', 12)
            .arg(geo[4], 0, 'f', 12).arg(geo[5], 0, 'f', 12);

    // SimpleSource 模板: 含 SrcRect/DstRect 像素映射 + SourceBand
    auto bandXml = [&](const QString& srcPath, int bandNum)
    {
        return QStringLiteral(
            "  <VRTRasterBand dataType=\"%1\" band=\"%2\">\n"
            "    <SimpleSource>\n"
            "      <SourceFilename relativeToVRT=\"0\">%3</SourceFilename>\n"
            "      <SourceBand>1</SourceBand>\n"
            "      <SrcRect xOff=\"0\" yOff=\"0\" xSize=\"%4\" ySize=\"%5\"/>\n"
            "      <DstRect xOff=\"0\" yOff=\"0\" xSize=\"%4\" ySize=\"%5\"/>\n"
            "    </SimpleSource>\n"
            "  </VRTRasterBand>\n")
            .arg(dataType)
            .arg(bandNum)
            .arg(srcPath.toHtmlEscaped())
            .arg(srcW).arg(srcH);
    };

    QString vrtXml = QStringLiteral(
        "<VRTDataset rasterXSize=\"%1\" rasterYSize=\"%2\">\n"
        "%3"  // SRS
        "%4"  // GeoTransform
        "%5"  // Band 1 (Red)
        "%6"  // Band 2 (Green)
        "%7"  // Band 3 (Blue)
        "</VRTDataset>\n")
        .arg(srcW).arg(srcH)
        .arg(srsXml.isEmpty() ? QString() : QStringLiteral("  ") + srsXml + QStringLiteral("\n"))
        .arg(geoXml)
        .arg(bandXml(rImg.rasterSourcePath, 1))
        .arg(bandXml(gImg.rasterSourcePath, 2))
        .arg(bandXml(bImg.rasterSourcePath, 3));

    // 写入临时 VRT 文件
    QString vrtPath = QDir::tempPath() + QStringLiteral("/rgb_%1_%2.vrt")
                      .arg(productId).arg(QDateTime::currentMSecsSinceEpoch());
    QFile vrtFile(vrtPath);
    if (!vrtFile.open(QIODevice::WriteOnly | QIODevice::Text)) return {};
    vrtFile.write(vrtXml.toUtf8());
    vrtFile.close();

    // 加载为 QGIS 图层
    QString displayName = QStringLiteral("%1 - RGB(%2-%3-%4)")
        .arg(productId)
        .arg(rImg.physicalBand).arg(gImg.physicalBand).arg(bImg.physicalBand);
    auto* rl = new QgsRasterLayer(vrtPath, displayName, QStringLiteral("gdal"));
    if (!rl->isValid())
    {
        qWarning() << "[LayerService] VRT layer invalid:" << vrtPath;
        delete rl;
        return {};
    }

    const QString newId = QStringLiteral("layer_%1_rgb_%2")
        .arg(mLayers.size()).arg(productId);

    RasterImage img;
    img.layerId          = newId;
    img.displayName      = displayName;
    img.filePath         = vrtPath;
    img.rasterSourcePath = vrtPath;
    img.productId        = productId;
    img.physicalBand     = 0;
    img.sensorType       = rImg.sensorType;
    img.bandCount        = 3;
    img.rasterSize       = QSize(srcW, srcH);
    img.dataType         = dataType;
    img.visible          = true;

    mLayers.prepend(img);
    mMapLayers.prepend(rl);
    mLayerOrder.prepend(newId);
    emit layerLoaded(newId, displayName, QStringLiteral("RGB"));

    rebuildCanvasLayers();
    emit renderLayersChanged(mLayers);

    qDebug() << "[LayerService] RGB composite created:" << newId << vrtPath;
    return newId;
}

// ─── Band Manager: RGB layer from product ─────────────────────────────────────

QString LayerServiceImpl::addRgbLayerFromPaths(const QString& rPath,
                                                const QString& gPath,
                                                const QString& bPath,
                                                const SensorInfo& info,
                                                const QString& displayId)
{
    GDALAllRegister();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALDataset* probeDS = (GDALDataset*)GDALOpen(rPath.toUtf8().constData(), GA_ReadOnly);
    if (!probeDS) { CPLPopErrorHandler(); return {}; }

    int srcW = probeDS->GetRasterXSize();
    int srcH = probeDS->GetRasterYSize();
    double geo[6] = {};
    probeDS->GetGeoTransform(geo);
    int epsg = -1;
    {
        const char* pWkt = probeDS->GetProjectionRef();
        if (pWkt && pWkt[0]) {
            OGRSpatialReference srs(pWkt); srs.AutoIdentifyEPSG();
            const char* code = srs.GetAuthorityCode(nullptr);
            if (code) epsg = QString::fromLatin1(code).toInt();
        }
    }
    QString dataType = QString::fromUtf8(
        GDALGetDataTypeName(probeDS->GetRasterBand(1)->GetRasterDataType()));
    if (dataType.isEmpty()) dataType = QStringLiteral("UInt16");
    // Normalize to north-up (QGIS expects geo[5] < 0)
    if (geo[5] > 0) {
        geo[3] = geo[3] + srcH * geo[5];  // move origin to top
        geo[5] = -geo[5];
    }
    CPLPopErrorHandler();
    GDALClose(probeDS);

    QString geoXml;
    if (geo[1] != 0.0 || geo[5] != 0.0)
        geoXml = QStringLiteral("  <GeoTransform>%1, %2, %3, %4, %5, %6</GeoTransform>\n")
            .arg(geo[0],0,'f',12).arg(geo[1],0,'f',12).arg(geo[2],0,'f',12)
            .arg(geo[3],0,'f',12).arg(geo[4],0,'f',12).arg(geo[5],0,'f',12);
    QString srsXml;
    if (epsg > 0)
        srsXml = QStringLiteral("  <SRS>EPSG:%1</SRS>\n").arg(epsg);

    // 从源波段文件中读取 GDAL 统计信息，用于 VRT 元数据（避免 GDAL/QGIS 重算时 dfMax<=dfMin）
    auto readBandStats = [](const QString& path) -> QString {
        GDALDataset* ds = (GDALDataset*)GDALOpen(path.toUtf8().constData(), GA_ReadOnly);
        if (!ds) return {};
        GDALRasterBand* band = ds->GetRasterBand(1);
        if (!band) { GDALClose(ds); return {}; }
        int bGot = FALSE;
        double dfMin = band->GetMinimum(&bGot);
        double dfMax = band->GetMaximum(&bGot);
        if (!bGot || dfMax <= dfMin)
        {
            dfMin = 0; dfMax = 0; double dfMean = 0; double dfStdDev = 0;
            CPLErr e = band->ComputeStatistics(FALSE, &dfMin, &dfMax, &dfMean, &dfStdDev, nullptr, nullptr);
            if (!(e == CE_None && dfMax > dfMin))
                band->ComputeStatistics(TRUE, &dfMin, &dfMax, &dfMean, &dfStdDev, nullptr, nullptr);
        }
        GDALClose(ds);
        if (dfMax <= dfMin) return {};
        return QStringLiteral(
            "    <Metadata>\n"
            "      <MDI key=\"STATISTICS_MINIMUM\">%1</MDI>\n"
            "      <MDI key=\"STATISTICS_MAXIMUM\">%2</MDI>\n"
            "    </Metadata>\n")
            .arg(dfMin, 0, 'f', 6).arg(dfMax, 0, 'f', 6);
    };

    auto bandXml = [&](const QString& path, int num) {
        QString statsMeta = readBandStats(path);
        return QStringLiteral(
            "  <VRTRasterBand dataType=\"%1\" band=\"%2\">\n"
            "%3"
            "    <SimpleSource>\n"
            "      <SourceFilename relativeToVRT=\"0\">%4</SourceFilename>\n"
            "      <SourceBand>1</SourceBand>\n"
            "      <SrcRect xOff=\"0\" yOff=\"0\" xSize=\"%5\" ySize=\"%6\"/>\n"
            "      <DstRect xOff=\"0\" yOff=\"0\" xSize=\"%5\" ySize=\"%6\"/>\n"
            "    </SimpleSource>\n"
            "  </VRTRasterBand>\n")
            .arg(dataType).arg(num).arg(statsMeta).arg(path).arg(srcW).arg(srcH);
    };

    QString vrtXml = QStringLiteral(
        "<VRTDataset rasterXSize=\"%1\" rasterYSize=\"%2\">\n%3%4%5%6%7</VRTDataset>\n")
        .arg(srcW).arg(srcH).arg(srsXml).arg(geoXml)
        .arg(bandXml(rPath,1)).arg(bandXml(gPath,2)).arg(bandXml(bPath,3));

    QString vrtPath = QDir::tempPath() + QStringLiteral("/bandmgr_rgb_%1_%2.vrt")
                        .arg(displayId).arg(QDateTime::currentMSecsSinceEpoch());
    QFile f(vrtPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return {};
    f.write(vrtXml.toUtf8()); f.close();

    auto* rl = new QgsRasterLayer(vrtPath,
        QStringLiteral("%1 - RGB").arg(displayId), QStringLiteral("gdal"));
    if (!rl->isValid()) { delete rl; return {}; }

    const QString newId = QStringLiteral("layer_%1_rgb_%2").arg(mLayers.size()).arg(displayId);

    RasterImage img;
    img.layerId = newId;
    img.displayName = QStringLiteral("%1 - RGB").arg(displayId);
    img.filePath = displayId;
    img.rasterSourcePath = vrtPath;
    img.productId = displayId;
    img.sensorType = info.sensorType;
    img.bandCount = 3;
    img.rasterSize = QSize(srcW, srcH);
    img.dataType = dataType;
    img.geoTransform = {geo[0],geo[1],geo[2],geo[3],geo[4],geo[5]};
    img.projectionWkt = epsg > 0 ? QStringLiteral("EPSG:%1").arg(epsg) : QString();
    img.epsgCode = epsg;
    img.visible = true;
    img.physicalBand = 0;

    mLayers.prepend(img);
    mMapLayers.prepend(rl);
    mLayerOrder.prepend(newId);
    emit layerLoaded(newId, img.displayName, QStringLiteral("RGB"));
    rebuildCanvasLayers();
    emit renderLayersChanged(mLayers);
    qDebug() << "[LayerService] RGB layer from paths:" << newId;
    return newId;
}

QString LayerServiceImpl::addRgbLayer(const QString& productPath,
                                       const BandConfiguration& cfg)
{
    if (!cfg.isValid())
    {
        emit layerError(productPath, QStringLiteral("Invalid band configuration"));
        return {};
    }

    QScopedPointer<ISensorProduct> prod(createSensorProduct(productPath));
    if (!prod || !prod->open(productPath))
    {
        emit layerError(productPath, QStringLiteral("Cannot open product for band selection"));
        return {};
    }

    const auto bands = prod->bands();
    if (bands.isEmpty())
    {
        emit layerError(productPath, QStringLiteral("Product has no bands"));
        return {};
    }

    const SensorInfo info = prod->sensorInfo();
    const QString productId = prod->productId();

    // Find raster paths for R/G/B physical bands
    auto findPath = [&](int physBand) -> QString {
        for (const auto& b : bands)
            if (b.physicalBand == physBand)
                return b.rasterPath;
        return {};
    };

    QString rPath = findPath(cfg.redBand);
    QString gPath = findPath(cfg.greenBand);
    QString bPath = findPath(cfg.blueBand);
    if (rPath.isEmpty() || gPath.isEmpty() || bPath.isEmpty())
    {
        emit layerError(productPath,
            QStringLiteral("Band not found: R=%1 G=%2 B=%3")
                .arg(cfg.redBand).arg(cfg.greenBand).arg(cfg.blueBand));
        return {};
    }

    // Probe spatial reference from red band
    // Sentinel-2 JP2 files trigger harmless WKT warnings from OpenJPEG driver
    GDALAllRegister();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALDataset* probeDS = (GDALDataset*)GDALOpen(rPath.toUtf8().constData(), GA_ReadOnly);
    if (!probeDS) { CPLPopErrorHandler(); emit layerError(productPath, QStringLiteral("Cannot open raster: %1").arg(rPath)); return {}; }

    int srcW = probeDS->GetRasterXSize();
    int srcH = probeDS->GetRasterYSize();
    double geo[6] = {};
    probeDS->GetGeoTransform(geo);
    int epsg = -1;
    {
        const char* pWkt = probeDS->GetProjectionRef();
        if (pWkt && pWkt[0])
        {
            OGRSpatialReference srs(pWkt);
            srs.AutoIdentifyEPSG();
            const char* code = srs.GetAuthorityCode(nullptr);
            if (code) epsg = QString::fromLatin1(code).toInt();
        }
    }

    QString dataType = QString::fromUtf8(
        GDALGetDataTypeName(probeDS->GetRasterBand(1)->GetRasterDataType()));
    if (dataType.isEmpty()) dataType = QStringLiteral("UInt16");
    GDALClose(probeDS);
    CPLPopErrorHandler();

    QString geoXml;
    if (geo[1] != 0.0 || geo[5] != 0.0)
        geoXml = QStringLiteral(
            "  <GeoTransform>%1, %2, %3, %4, %5, %6</GeoTransform>\n")
            .arg(geo[0], 0, 'f', 12).arg(geo[1], 0, 'f', 12)
            .arg(geo[2], 0, 'f', 12).arg(geo[3], 0, 'f', 12)
            .arg(geo[4], 0, 'f', 12).arg(geo[5], 0, 'f', 12);

    QString srsXml;
    if (epsg > 0)
        srsXml = QStringLiteral("  <SRS>EPSG:%1</SRS>\n").arg(epsg);

    auto bandXml = [&](const QString& srcPath, int bandNum) {
        return QStringLiteral(
            "  <VRTRasterBand dataType=\"%1\" band=\"%2\">\n"
            "    <SimpleSource>\n"
            "      <SourceFilename relativeToVRT=\"0\">%3</SourceFilename>\n"
            "      <SourceBand>1</SourceBand>\n"
            "      <SrcRect xOff=\"0\" yOff=\"0\" xSize=\"%4\" ySize=\"%5\"/>\n"
            "      <DstRect xOff=\"0\" yOff=\"0\" xSize=\"%4\" ySize=\"%5\"/>\n"
            "    </SimpleSource>\n"
            "  </VRTRasterBand>\n")
            .arg(dataType).arg(bandNum).arg(srcPath.toHtmlEscaped())
            .arg(srcW).arg(srcH);
    };

    QString vrtXml = QStringLiteral(
        "<VRTDataset rasterXSize=\"%1\" rasterYSize=\"%2\">\n"
        "%3%4%5%6%7"
        "</VRTDataset>\n")
        .arg(srcW).arg(srcH).arg(srsXml).arg(geoXml)
        .arg(bandXml(rPath, 1))
        .arg(bandXml(gPath, 2))
        .arg(bandXml(bPath, 3));

    QString vrtPath = QDir::tempPath() + QStringLiteral("/bandmgr_rgb_%1_%2.vrt")
                        .arg(productId).arg(QDateTime::currentMSecsSinceEpoch());
    QFile vrtFile(vrtPath);
    if (!vrtFile.open(QIODevice::WriteOnly | QIODevice::Text)) return {};
    vrtFile.write(vrtXml.toUtf8());
    vrtFile.close();

    QString displayName = QStringLiteral("%1 - RGB(%2-%3-%4)")
        .arg(productId).arg(cfg.redBand).arg(cfg.greenBand).arg(cfg.blueBand);
    auto* rl = new QgsRasterLayer(vrtPath, displayName, QStringLiteral("gdal"));
    if (!rl->isValid())
    {
        qWarning() << "[LayerService] VRT layer invalid:" << vrtPath;
        delete rl;
        emit layerError(productPath, QStringLiteral("VRT layer is invalid"));
        return {};
    }

    const QString newId = QStringLiteral("layer_%1_rgb_%2")
        .arg(mLayers.size()).arg(productId);

    RasterImage img;
    img.layerId          = newId;
    img.displayName      = displayName;
    img.filePath         = productPath;  // original product path, not VRT path
    img.rasterSourcePath = vrtPath;
    img.productId        = productId;
    img.sensorType       = info.sensorType;
    img.bandCount        = 3;
    img.rasterSize       = QSize(srcW, srcH);
    img.dataType         = dataType;
    img.geoTransform     = {geo[0], geo[1], geo[2], geo[3], geo[4], geo[5]};
    img.projectionWkt    = epsg > 0 ? QStringLiteral("EPSG:%1").arg(epsg) : QString();
    img.epsgCode         = epsg;
    img.visible          = true;

    // Encode BandConfiguration in physicalBand for later reconfiguration
    img.physicalBand = (cfg.redBand << 16) | (cfg.greenBand << 8) | cfg.blueBand;

    mLayers.prepend(img);
    mMapLayers.prepend(rl);
    mLayerOrder.prepend(newId);
    emit layerLoaded(newId, displayName, QStringLiteral("RGB"));

    rebuildCanvasLayers();
    emit renderLayersChanged(mLayers);

    qDebug() << "[LayerService] RGB layer added via Band Manager:" << newId
             << "R=" << cfg.redBand << "G=" << cfg.greenBand << "B=" << cfg.blueBand;
    return newId;
}

bool LayerServiceImpl::reconfigureRgb(const QString& layerId,
                                       const BandConfiguration& cfg)
{
    if (!cfg.isValid()) return false;

    int idx = layerIndex(layerId);
    if (idx < 0) return false;

    // Get original product path (stored in filePath)
    QString productPath = mLayers[idx].filePath;
    if (productPath.isEmpty() || !QFileInfo::exists(productPath))
    {
        qWarning() << "[LayerService] reconfigureRgb: product path lost";
        return false;
    }

    // Remove old layer
    QString newId = addRgbLayer(productPath, cfg);
    if (newId.isEmpty()) return false;

    // Remove the old RGB layer (the new one is prepended, so find old by its id)
    removeLayers({layerId});

    qDebug() << "[LayerService] RGB reconfigured:" << layerId << "→" << newId;
    return true;
}

// ─── 矢量图层 ────────────────────────────────────────────────────────────────

int LayerServiceImpl::vectorLayerIndex(const QString& layerId) const
{
    for (int i = 0; i < mVectorLayerInfos.size(); ++i)
        if (mVectorLayerInfos[i].layerId == layerId) return i;
    return -1;
}

QStringList LayerServiceImpl::addVectorLayers(const QStringList& filePaths)
{
    qDebug() << "[LayerService] addVectorLayers:" << filePaths;
    QStringList addedIds;

    if (!mVectorReader)
    {
        qWarning() << "[LayerService] addVectorLayers: no IVectorReader set";
        for (const QString& path : filePaths)
            emit layerError(path, QStringLiteral("矢量读取器未初始化"));
        return addedIds;
    }

    for (const QString& path : filePaths)
    {
        if (!mVectorReader->open(path))
        {
            emit layerError(path, QStringLiteral("无法打开矢量文件: ") + path);
            continue;
        }

        const int nLayers = mVectorReader->layerCount();
        if (nLayers == 0)
        {
            emit layerError(path, QStringLiteral("矢量文件中没有图层"));
            mVectorReader->close();
            continue;
        }

        for (int lyrIdx = 0; lyrIdx < nLayers; ++lyrIdx)
        {
            const QString layerName = mVectorReader->layerNames().value(lyrIdx);
            const QString displayName = nLayers > 1
                ? QStringLiteral("%1 - %2").arg(QFileInfo(path).fileName(), layerName)
                : QFileInfo(path).fileName();

            const QString id = QStringLiteral("vec_%1_%2")
                .arg(mVectorLayerInfos.size())
                .arg(layerName.isEmpty() ? QString::number(lyrIdx) : layerName);

            VectorLayerInfo info = mVectorReader->toVectorLayerInfo(id, displayName, lyrIdx);
            info.filePath = path;

            auto* vl = new QgsVectorLayer(path, displayName, QStringLiteral("ogr"));
            if (!vl->isValid())
            {
                qWarning() << "[LayerService] QGIS failed to load vector:" << path;
                emit layerError(id, QStringLiteral("QGIS 无法加载矢量图层: ") + displayName);
                delete vl;
                continue;
            }

            mVectorLayerInfos.prepend(info);
            mVectorMapLayers.prepend(vl);
            mLayerOrder.prepend(id);
            addedIds.append(id);
            emit vectorLayerLoaded(id, displayName, info.geometryType);
            qDebug() << "[LayerService] vector layer added:" << id
                     << "features:" << info.featureCount
                     << "type:" << info.geometryType;
        }

        mVectorReader->close();
    }

    if (!addedIds.isEmpty())
    {
        rebuildCanvasLayers();
        emit vectorRenderLayersChanged(mVectorLayerInfos);
    }
    return addedIds;
}

QgsMapLayer* LayerServiceImpl::mapLayer(const QString& layerId) const
{
    return findMapLayer(layerId);
}

VectorLayerInfo LayerServiceImpl::vectorLayerInfo(const QString& layerId) const
{
    for (const auto& info : mVectorLayerInfos)
    {
        if (info.layerId == layerId)
            return info;
    }
    return {};
}

static QColor colorForIndex(int idx, int total)
{
    // Equal-spaced hues around the color wheel for visual distinctness
    const int hue = (idx * 360 / qMax(total, 1)) % 360;
    return QColor::fromHsv(hue, 200, 240);
}

void LayerServiceImpl::setVectorStyle(const QString& layerId, const VectorStyleConfig& style)
{
    const int idx = vectorLayerIndex(layerId);
    if (idx < 0 || idx >= mVectorMapLayers.size())
        return;

    auto* vl = qobject_cast<QgsVectorLayer*>(mVectorMapLayers[idx]);
    if (!vl)
        return;

    if (style.styleType == VectorStyleType::SingleSymbol)
    {
        // ── 单一符号 ──
        const int geomType = static_cast<int>(vl->geometryType());
        QgsSymbol* symbol = nullptr;

        if (geomType == 0)
        {
            QVariantMap props;
            props[QStringLiteral("color")] = style.strokeColor.name();
            props[QStringLiteral("size")]  = QString::number(style.markerSize);
            symbol = QgsMarkerSymbol::createSimple(props);
        }
        else if (geomType == 1)
        {
            QVariantMap props;
            props[QStringLiteral("color")] = style.strokeColor.name();
            props[QStringLiteral("width")] = QString::number(style.strokeWidth);
            symbol = QgsLineSymbol::createSimple(props);
        }
        else
        {
            QVariantMap props;
            props[QStringLiteral("color")]         = style.fillColor.name();
            props[QStringLiteral("outline_color")] = style.strokeColor.name();
            props[QStringLiteral("outline_width")] = QString::number(style.strokeWidth);
            symbol = QgsFillSymbol::createSimple(props);
        }

        if (symbol)
        {
            vl->setRenderer(new QgsSingleSymbolRenderer(symbol));
            if (mCanvas) mCanvas->refresh();
        }
        return;
    }

    // ── 加载字段数据 ──
    if (!mVectorReader)
        return;

    const int vLyrIdx = 0;  // single-layer files; multi-layer handled on open
    const QString filePath = mVectorLayerInfos[idx].filePath;
    if (!mVectorReader->open(filePath))
        return;

    const int fieldIdx = vl->fields().lookupField(style.classifyField);
    if (fieldIdx < 0)
    {
        mVectorReader->close();
        qWarning() << "[LayerService] classify field not found:" << style.classifyField;
        return;
    }

    if (style.styleType == VectorStyleType::Categorized)
    {
        // ── 分类着色 ──
        const QStringList uniqVals = mVectorReader->uniqueValues(
            vLyrIdx, style.classifyField);
        if (uniqVals.isEmpty())
        {
            mVectorReader->close();
            return;
        }

        QgsCategoryList categories;
        for (int i = 0; i < uniqVals.size(); ++i)
        {
            QgsSymbol* sym = QgsSymbol::defaultSymbol(vl->geometryType());
            sym->setColor(colorForIndex(i, uniqVals.size()));
            categories << QgsRendererCategory(QVariant(uniqVals[i]), sym, uniqVals[i]);
        }
        mVectorReader->close();

        auto* renderer = new QgsCategorizedSymbolRenderer(style.classifyField, categories);
        renderer->setSourceColorRamp(new QgsRandomColorRamp());
        vl->setRenderer(renderer);
        if (mCanvas) mCanvas->refresh();
    }
    else if (style.styleType == VectorStyleType::Graduated)
    {
        // ── 渐变着色 ──
        double minVal = 0.0, maxVal = 0.0;
        if (!mVectorReader->numericFieldRange(vLyrIdx, style.classifyField, minVal, maxVal)
            || qFuzzyCompare(minVal, maxVal))
        {
            mVectorReader->close();
            return;
        }

        QgsClassificationMethod* method = nullptr;
        const QString& m = style.classificationMethod;
        if (m == QStringLiteral("Jenks"))
            method = new QgsClassificationJenks();
        else if (m == QStringLiteral("EqualInterval"))
            method = new QgsClassificationEqualInterval();
        else if (m == QStringLiteral("Quantile"))
            method = new QgsClassificationQuantile();
        else if (m == QStringLiteral("StdDev"))
            method = new QgsClassificationStandardDeviation();
        else
            method = new QgsClassificationJenks();

        method->setLabelFormat(QStringLiteral("%1 - %2"));
        const QList<QgsClassificationRange> classes = method->classes(
            minVal, maxVal, style.classCount);
        delete method;

        QgsRangeList rangeList;
        for (int i = 0; i < classes.size(); ++i)
        {
            QgsSymbol* sym = QgsSymbol::defaultSymbol(vl->geometryType());
            sym->setColor(colorForIndex(i, classes.size()));
            const QString label = QStringLiteral("%1 - %2")
                .arg(classes[i].lowerBound(), 0, 'f', 2)
                .arg(classes[i].upperBound(), 0, 'f', 2);
            rangeList << QgsRendererRange(classes[i].lowerBound(),
                                           classes[i].upperBound(), sym, label);
        }
        mVectorReader->close();

        auto* renderer = new QgsGraduatedSymbolRenderer(style.classifyField, rangeList);
        renderer->setSourceColorRamp(
            new QgsGradientColorRamp(QColor(255, 255, 200), QColor(200, 0, 0)));
        vl->setRenderer(renderer);
        if (mCanvas) mCanvas->refresh();
    }
}

// ─── 导出图层 ────────────────────────────────────────────────────────────────

// Extract a single file from inside a ZIP to a temp directory, return local path
static QString extractVsizipFile(const QString& vsizipPath, const QString& tempDir)
{
    // vsizipPath like: "/vsizip/F:/data.zip/path/inside/file.jp2"
    QString inner = vsizipPath;
    if (inner.startsWith(QStringLiteral("/vsizip/")))
        inner = inner.mid(8);  // remove "/vsizip/"
    // Now: "F:/data.zip/path/inside/file.jp2"
    // Split at first ".zip/" or ".ZIP/"
    int zipEnd = inner.indexOf(QStringLiteral(".zip/"), 0, Qt::CaseInsensitive);
    if (zipEnd < 0) zipEnd = inner.indexOf(QStringLiteral(".SAFE/"), 0, Qt::CaseInsensitive);
    if (zipEnd < 0) return vsizipPath;  // can't parse, use as-is

    QString zipPath = inner.left(zipEnd + 4);     // "F:/data.zip"
    QString fileInZip = inner.mid(zipEnd + 5);     // "path/inside/file.jp2"
    QString fullVsizip = QStringLiteral("/vsizip/") + zipPath + QStringLiteral("/") + fileInZip;
    QString outPath = tempDir + QStringLiteral("/") + QFileInfo(fileInZip).fileName();

    VSILFILE* in = VSIFOpenL(fullVsizip.toUtf8().constData(), "rb");
    if (!in) return vsizipPath;
    VSILFILE* out = VSIFOpenL(outPath.toUtf8().constData(), "wb");
    if (!out) { VSIFCloseL(in); return vsizipPath; }

    char buf[65536];
    size_t n;
    while ((n = VSIFReadL(buf, 1, sizeof(buf), in)) > 0)
        VSIFWriteL(buf, 1, n, out);
    VSIFCloseL(in);
    VSIFCloseL(out);
    return outPath;
}

// If srcPath is a VRT that references /vsizip/ files, extract them and rewrite VRT
static QString maybeUnpackVrt(const QString& srcPath, const QString& tempDir)
{
    QFile f(srcPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return srcPath;

    QString xml = QString::fromUtf8(f.readAll());
    f.close();

    if (!xml.contains(QStringLiteral("/vsizip/")))
        return srcPath;  // no ZIP sources, use as-is

    // Find all /vsizip/... paths and extract
    QRegularExpression re(QStringLiteral("/vsizip/[^\\s\"<>]+"));
    QSet<QString> seen;
    auto it = re.globalMatch(xml);
    while (it.hasNext())
    {
        auto m = it.next();
        QString orig = m.captured();
        if (seen.contains(orig)) continue;
        seen.insert(orig);
        QString local = extractVsizipFile(orig, tempDir);
        if (local != orig)
            xml.replace(orig, local);
    }

    QString newVrt = tempDir + QStringLiteral("/_unpacked.vrt");
    QFile out(newVrt);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return srcPath;
    out.write(xml.toUtf8());
    out.close();
    return newVrt;
}

bool LayerServiceImpl::exportLayer(const QString& layerId, const QString& outputPath,
                                     const ExportOptions& options)
{
    RasterImage img = layerImage(layerId);
    QString srcPath = img.rasterSourcePath;
    if (srcPath.isEmpty()) srcPath = img.filePath;
    if (srcPath.isEmpty()) return false;

    GDALAllRegister();

    // Extract JP2 sources from ZIP to temp for faster random access
    QString tempDir = QDir::tempPath() + QStringLiteral("/gdal_export_")
                      + QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(tempDir);
    QString actualSrc = maybeUnpackVrt(srcPath, tempDir);

    GDALDataset* srcDS = (GDALDataset*)GDALOpen(actualSrc.toUtf8().constData(), GA_ReadOnly);
    if (!srcDS)
    {
        qWarning() << "[LayerService] exportLayer: cannot open source:" << actualSrc;
        QDir(tempDir).removeRecursively();
        return false;
    }

    char** createOpts = nullptr;
    if (options.compression != QStringLiteral("NONE"))
    {
        createOpts = CSLSetNameValue(createOpts, "COMPRESS",
                                     options.compression.toUtf8().constData());
        if (options.usePredictor)
            createOpts = CSLSetNameValue(createOpts, "PREDICTOR", "2");
        createOpts = CSLSetNameValue(createOpts, "TILED", "YES");
    }
    createOpts = CSLSetNameValue(createOpts, "BLOCKXSIZE", "512");
    createOpts = CSLSetNameValue(createOpts, "BLOCKYSIZE", "512");
    int threads = options.numThreads > 0 ? options.numThreads : QThread::idealThreadCount();
    createOpts = CSLSetNameValue(createOpts, "NUM_THREADS",
                                 QString::number(threads).toUtf8().constData());

    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset* dstDS = driver->CreateCopy(outputPath.toUtf8().constData(), srcDS,
                                              FALSE, createOpts, nullptr, nullptr);
    CSLDestroy(createOpts);
    bool ok = (dstDS != nullptr);
    GDALClose(srcDS);

    if (ok)
    {
        if (options.buildPyramids)
        {
            GDALRasterBand* band1 = dstDS->GetRasterBand(1);
            if (band1 && band1->GetOverviewCount() == 0)
            {
                int anOverviews[] = { 2, 4, 8, 16, 32 };
                GDALBuildOverviews(GDALDatasetH(dstDS), "NEAREST",
                                   5, anOverviews, 0, nullptr, nullptr, nullptr);
            }
        }
        GDALClose(dstDS);
        qDebug() << "[LayerService] layer exported:" << outputPath;
    }
    else
    {
        qWarning() << "[LayerService] exportLayer: CreateCopy failed:" << outputPath;
    }

    // Clean up temp extracted files
    QDir(tempDir).removeRecursively();
    return ok;
}
