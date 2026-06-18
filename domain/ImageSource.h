#ifndef IMAGESOURCE_H
#define IMAGESOURCE_H

#include <QString>
#include <QList>
#include <QVariantMap>
#include <QMetaType>

enum class ImageRole
{
    MosaicInput,      // 镶嵌输入 — 参与拼接的普通影像
    Panchromatic,     // 全色波段 — 用于影像融合
    Reference,        // 几何参考 — 用于几何校正的基准影像
    HistReference     // 匀色参考 — 用于镶嵌匀色的参考影像
};

struct BandSelection
{
    QString purpose;            // 用途标识: "Multispectral", "Panchromatic", "NDVI" ...
    QList<int> bandNumbers;     // 物理波段号, 如 [4, 3, 2]

    bool isValid() const { return !purpose.isEmpty() && !bandNumbers.isEmpty(); }
};

struct ImageSource
{
    QString sourceId;               // 工程内唯一标识
    QString displayName;            // 用户可读名称
    QString filePath;               // 原始文件路径 (.zip / .SAFE / .tif)
    QString sensorType;             // 传感器类型, 自动检测: "Landsat-8" / "Sentinel-2A" / "GF-2"
    ImageRole role = ImageRole::MosaicInput;
    QList<BandSelection> bandSelections;   // 需要提取的波段组合
    QVariantMap metadata;                  // 自动提取的元数据快照 (分辨率/投影/范围等)

    bool isValid() const { return !filePath.isEmpty(); }
};

Q_DECLARE_METATYPE(ImageSource)
Q_DECLARE_METATYPE(BandSelection)

#endif // IMAGESOURCE_H
