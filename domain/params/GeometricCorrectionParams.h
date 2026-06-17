#ifndef GEOMETRICCORRECTIONPARAMS_H
#define GEOMETRICCORRECTIONPARAMS_H

#include <QString>
#include <QVector>
#include <QMetaType>
#include "algorithms/common/AlgorithmResult.h"

// ── 地面控制点 ──
struct Gcp
{
    double srcX = 0, srcY = 0;       // 源影像像素坐标
    double refX = 0, refY = 0;       // 参考坐标（像素或地理坐标）
    double residual = 0;             // 拟合后残差（像素）
    bool   isAuto = false;           // 是否由算法自动检测
};

// ── 匹配参数 ──
struct GcpMatchingParams
{
    QString method = "SIFT";         // "SIFT" / "SURF" / "NCC" / "Manual"
    double ratioThreshold = 0.7;     // Lowe's ratio test 阈值
    double ransacThreshold = 3.0;    // RANSAC 重投影误差阈值（像素）
    int    maxFeatures = 5000;       // 每幅影像最大特征点数
    int    nccTemplateSize = 64;     // NCC 模板大小（半自动模式）
    int    nccSearchWindow = 128;    // NCC 搜索窗口半边长
};

// ── 校正模型 ──
struct GcpModel
{
    QVector<double> coefficients;    // 多项式系数[a0,a1,...] 或 TPS 权重
    QString type;                    // "Polynomial1"~"5" / "TPS"
    double overallRmse = 0;
    int    minGcpCount = 0;          // 该模型所需最少 GCP 数
};

// ── 校正参数 ──
struct GeometricCorrectionParams
{
    QString sourceImage;
    QString referenceImage;
    QString outputPath;

    // GCP
    QString matchingMode = "Auto";   // "Auto" / "SemiAuto" / "Manual"
    GcpMatchingParams matching;
    QVector<Gcp> gcps;               // 手动/半自动时预填充

    // 模型
    QString modelType = "Polynomial2";
    QString resampleMethod = "Bilinear"; // "Nearest" / "Bilinear" / "Cubic"

    // 输出
    QString outputProjection;        // "EPSG:xxxx" 或留空=使用源投影
    double outputPixelSizeX = 0;     // 0 = 自动从参考影像读取
    double outputPixelSizeY = 0;
    double outputExtent[4] = {0, 0, 0, 0}; // minX, minY, maxX, maxY; 0=自动计算
    int    blockSize = 512;
};

// ── 校正结果报告 ──
struct GeometricResult : AlgorithmResult
{
    QVector<Gcp> finalGcps;          // 最终参与拟合的 GCP（含残差）
    GcpModel model;
    int    totalGcps = 0;            // 匹配到的原始 GCP 数
    int    inlierGcps = 0;           // RANSAC 后的内点数
    double matchTimeSec = 0;
    double correctTimeSec = 0;
    bool   gpuUsed = false;
};

Q_DECLARE_METATYPE(GeometricCorrectionParams)

#endif // GEOMETRICCORRECTIONPARAMS_H
