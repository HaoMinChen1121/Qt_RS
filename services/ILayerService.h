#ifndef ILAYERSERVICE_H
#define ILAYERSERVICE_H

#include <QObject>
#include <QStringList>
#include <QRectF>
#include <QList>
#include <QPair>

class QgsMapLayer;
#include "domain/RasterImage.h"
#include "domain/VectorLayerInfo.h"
#include "domain/VectorStyle.h"
#include "domain/SensorInfo.h"
#include "domain/BandConfiguration.h"

struct ExportOptions
{
    QString compression = QStringLiteral("LZW");  // "NONE" / "LZW" / "DEFLATE" / "PACKBITS"
    bool usePredictor = true;                      // PREDICTOR=2, 对 16-bit 数据效果明显
    bool buildPyramids = true;                     // 构建金字塔概视图
    int numThreads = 0;                            // 0=auto (ALL_CPUS)
};

class ILayerService : public QObject
{
    Q_OBJECT
public:
    virtual QStringList addLayers(const QStringList& filePaths) = 0;
    virtual void removeLayers(const QStringList& layerIds) = 0;
    virtual void setLayerVisibility(const QString& layerId, bool visible) = 0;
    virtual double layerOpacity(const QString& layerId) const = 0;
    virtual void setLayerOpacity(const QString& layerId, double opacity) = 0;
    virtual void reorderLayers(const QStringList& orderedIds) = 0;
    virtual QRectF layerExtent(const QString& layerId) const = 0;
    virtual RasterImage layerImage(const QString& layerId) const = 0;

    // 波段操作
    virtual QStringList splitToBands(const QString& layerId) = 0;
    virtual void setBandRenderer(const QString& layerId, int red, int green, int blue) = 0;
    virtual int bandCount(const QString& layerId) const = 0;
    virtual QString sensorTypeForLayer(const QString& layerId) const = 0;

    // 产品级波段操作
    virtual QList<QPair<QString, QString>> productBands(const QString& productId) const = 0;
    virtual QString createRgbComposite(const QString& productId,
                                        const QString& redLayerId,
                                        const QString& greenLayerId,
                                        const QString& blueLayerId) = 0;

    // RGB layer from product (Band Manager path — single VRT layer)
    virtual QString addRgbLayer(const QString& productPath,
                                const BandConfiguration& cfg) = 0;
    virtual QString addRgbLayerFromPaths(const QString& rPath,
                                          const QString& gPath,
                                          const QString& bPath,
                                          const SensorInfo& info,
                                          const QString& displayId) = 0;
    virtual bool reconfigureRgb(const QString& layerId,
                                const BandConfiguration& cfg) = 0;

    // 矢量图层
    virtual QStringList addVectorLayers(const QStringList& filePaths) = 0;
    virtual VectorLayerInfo vectorLayerInfo(const QString& layerId) const = 0;
    virtual void setVectorStyle(const QString& layerId, const VectorStyleConfig& style) = 0;
    virtual QgsMapLayer* mapLayer(const QString& layerId) const = 0;

    // 导出
    virtual bool exportLayer(const QString& layerId, const QString& outputPath,
                              const ExportOptions& options = ExportOptions()) = 0;

signals:
    void layerLoaded(const QString& layerId, const QString& name, const QString& type);
    void layerRemoved(const QString& layerId);
    void layerError(const QString& layerId, const QString& errorMessage);
    void renderLayersChanged(const QList<RasterImage>& layers);
    void vectorLayerLoaded(const QString& layerId, const QString& name, const QString& geometryType);
    void vectorRenderLayersChanged(const QList<VectorLayerInfo>& layers);
};

#endif // ILAYERSERVICE_H
