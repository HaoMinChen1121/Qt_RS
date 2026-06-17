#ifndef DESCRIPTORPRODUCT_H
#define DESCRIPTORPRODUCT_H

#include "dataaccess/ISensorProduct.h"
#include "domain/ProductDescriptor.h"

/// 从通用 .rpp 描述符重建的 ISensorProduct 实现
/// 不依赖任何传感器专有格式解析——所有波段与元数据均来自描述符 JSON
class DescriptorProduct : public ISensorProduct
{
public:
    bool open(const QString& path) override;
    void close() override;
    bool isOpen() const override;

    QList<RasterBandDescriptor> bands() const override;
    QList<RasterBandDescriptor> bandsAtResolution(double res) const override;

    SensorInfo sensorInfo() const override;
    QString sensorType() const override;
    QString productId() const override;

    QString previewImagePath() const override;
    QString originalPath() const override;

    /// 返回内部描述符引用，供 Worker 填充后写入
    ProductDescriptor& descriptor() { return mDescriptor; }

    /// 从任意 ISensorProduct 构建描述符（含波段枚举和元数据提取）
    static ProductDescriptor buildDescriptor(ISensorProduct* product,
                                             const QString& originalPath = {});

private:
    ProductDescriptor mDescriptor;
    QList<RasterBandDescriptor> mBands;
    SensorInfo         mSensorInfo;
    QString            mOriginalPath;
    bool               mOpen = false;
};

#endif // DESCRIPTORPRODUCT_H
