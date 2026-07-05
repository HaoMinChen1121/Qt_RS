#ifndef LAYERSERVICEIMPL_H
#define LAYERSERVICEIMPL_H

#include "services/ILayerService.h"
#include "domain/VectorLayerInfo.h"
#include "domain/VectorStyle.h"
#include <QList>

class IRasterReader;
class IVectorReader;
class ISensorProduct;
class QgsMapCanvas;
class QgsMapLayer;

class LayerServiceImpl : public ILayerService
{
    Q_OBJECT
public:
    explicit LayerServiceImpl(IRasterReader* reader, IVectorReader* vecReader = nullptr,
                              QObject* parent = nullptr);
    ~LayerServiceImpl() override;

    void setMapCanvas(QgsMapCanvas* canvas);

    QStringList addLayers(const QStringList& filePaths) override;
    void removeLayers(const QStringList& layerIds) override;
    void setLayerVisibility(const QString& layerId, bool visible) override;
    double layerOpacity(const QString& layerId) const override;
    void setLayerOpacity(const QString& layerId, double opacity) override;
    void reorderLayers(const QStringList& orderedIds) override;
    QRectF layerExtent(const QString& layerId) const override;
    RasterImage layerImage(const QString& layerId) const override;

    // 波段操作
    QStringList splitToBands(const QString& layerId) override;
    void setBandRenderer(const QString& layerId, int red, int green, int blue) override;
    int bandCount(const QString& layerId) const override;
    QString sensorTypeForLayer(const QString& layerId) const override;

    // 产品级波段操作
    QList<QPair<QString, QString>> productBands(const QString& productId) const override;
    QString createRgbComposite(const QString& productId,
                                const QString& redLayerId,
                                const QString& greenLayerId,
                                const QString& blueLayerId) override;

    // RGB layer from product (Band Manager)
    QString addRgbLayer(const QString& productPath,
                        const BandConfiguration& cfg) override;
    QString addRgbLayerFromPaths(const QString& rPath,
                                  const QString& gPath,
                                  const QString& bPath,
                                  const SensorInfo& info,
                                  const QString& displayId) override;
    bool reconfigureRgb(const QString& layerId,
                        const BandConfiguration& cfg) override;

    // 矢量图层
    QStringList addVectorLayers(const QStringList& filePaths) override;
    VectorLayerInfo vectorLayerInfo(const QString& layerId) const override;
    void setVectorStyle(const QString& layerId, const VectorStyleConfig& style) override;
    QgsMapLayer* mapLayer(const QString& layerId) const override;

    // 导出
    bool exportLayer(const QString& layerId, const QString& outputPath,
                      const ExportOptions& options = ExportOptions()) override;

private:
    void rebuildCanvasLayers();
    QgsMapLayer* findMapLayer(const QString& layerId) const;
    int layerIndex(const QString& layerId) const;
    int vectorLayerIndex(const QString& layerId) const;

    IRasterReader* mReader;
    IVectorReader* mVectorReader;
    QgsMapCanvas* mCanvas = nullptr;
    QList<RasterImage> mLayers;
    QList<QgsMapLayer*> mMapLayers;
    QList<VectorLayerInfo> mVectorLayerInfos;
    QList<QgsMapLayer*> mVectorMapLayers;
    QStringList mLayerOrder;  // unified display order: all layer IDs (raster + vector)
};

#endif // LAYERSERVICEIMPL_H
