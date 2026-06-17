#ifndef MOSAICPARAMS_H
#define MOSAICPARAMS_H

#include <QString>
#include <QStringList>
#include <QMetaType>

/**
 * @brief 镶嵌参数结构体（纯数据载体，不包含业务逻辑）
 */
struct MosaicParams
{
    // 输入影像
    QStringList inputImages;         // 待镶嵌影像路径列表

    // 匀色处理
    QString colorBalanceMethod;      // "None" / "HistogramMatching" / "WallisFilter" / "LUT"
    QString histogramReferenceImage; // 直方图匹配参考影像

    // Wallis滤波参数
    int wallisWindowSize = 127;      // 滤波窗口大小
    double wallisContrast = 1.0;     // 对比度拉伸系数
    double wallisBrightness = 0.5;   // 亮度系数

    // 拼接线
    QString seamlineMethod = "Voronoi"; // "None" / "Voronoi" / "MinCostPath" / "Manual"
    double seamlineEdgeWeight = 1.0; // 边缘代价权重
    double seamlineColorWeight = 1.0;// 颜色差异权重
    double seamlineTextureWeight = 0.5; // 纹理代价权重

    // 羽化
    int featheringWidth = 10;        // 羽化宽度(像素)
    QString featheringType = "Linear"; // "Linear" / "Sine" / "None"

    // 输出参数
    bool useImageExtent = true;      // 使用影像范围
    double outputExtentMinX = 0.0;
    double outputExtentMinY = 0.0;
    double outputExtentMaxX = 0.0;
    double outputExtentMaxY = 0.0;
    QString outputProjection;        // 输出投影 (EPSG:xxxx)
    double outputResolutionX = 0.0;   // 0 = 自动从源影像检测
    double outputResolutionY = 0.0;   // 0 = 自动从源影像检测
    QString outputFormat = "GeoTIFF";
    int backgroundValue = 0;         // 背景填充值
    int blockSize = 512;             // 分块处理大小(像素)

    // 输出
    QString outputPath;
};

Q_DECLARE_METATYPE(MosaicParams)

#endif // MOSAICPARAMS_H
