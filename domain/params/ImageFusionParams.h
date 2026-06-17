#ifndef IMAGEFUSIONPARAMS_H
#define IMAGEFUSIONPARAMS_H

#include <QString>
#include <QList>
#include <QMetaType>

/**
 * @brief 融合质量评价结果
 */
struct FusionQualityMetrics
{
    double correlationCoefficient = 0.0;  // 相关系数
    double averageGradient = 0.0;         // 平均梯度
    double rmse = 0.0;                    // 均方根误差
    double ergas = 0.0;                   // ERGAS指标
    double sam = 0.0;                     // 光谱角映射
    double ssim = 0.0;                    // 结构相似性
    double uiqi = 0.0;                    // 通用图像质量指标
};

Q_DECLARE_METATYPE(FusionQualityMetrics)

/**
 * @brief 图像融合参数结构体（纯数据载体，不包含业务逻辑）
 */
struct ImageFusionParams
{
    // 输入数据
    QString panchromaticImage;       // 全色影像路径
    QString multispectralImage;      // 多光谱影像路径
    QList<int> msBandIndices;        // 需要融合的多光谱波段索引

    // 算法选择
    QString algorithm;               // "IHS" / "Brovey" / "GramSchmidt" / "PCA" / "HPF" / "Wavelet"

    // IHS参数
    QString ihsColorModel = "HSI";   // "HSI" / "HSV" / "RGB"
    QString ihsStretchType = "Linear"; // 拉伸方式

    // Brovey参数
    QList<double> broveyBandWeights; // 各波段权重

    // Gram-Schmidt参数
    QString gsSimulationMethod = "Average";  // 模拟全色方法: "Average" / "SpectralResponse"
    QString gsSensorType;            // 传感器类型

    // PCA参数
    int pcaComponentCount = 1;       // 保留的主成分数目

    // HPF参数
    int hpfKernelSize = 5;           // 高通滤波核大小
    double hpfWeight = 0.5;          // 高通滤波权重

    // Wavelet参数
    int waveletDecompositionLevel = 3;    // 小波分解级数
    QString waveletType = "Daubechies4";  // "Daubechies4" / "Haar" / "Symlet8"

    // 质量评价开关
    bool computeCorrelationCoefficient = true;
    bool computeAverageGradient = true;
    bool computeRMSE = false;
    bool computeERGAS = false;
    bool computeSAM = false;
    bool computeSSIM = false;
    bool computeUIQI = false;

    // 输出
    QString outputPath;
};

#endif // IMAGEFUSIONPARAMS_H
