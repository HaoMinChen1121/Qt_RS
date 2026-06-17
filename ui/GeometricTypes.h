#ifndef GEOMETRICTYPES_H
#define GEOMETRICTYPES_H

#include <QString>
#include <QVector>

// ── 地面控制点（UI 展示用，映射到 domain Gcp） ──
struct GcpEntry
{
    int    id = 0;
    double srcX = 0, srcY = 0;
    double refX = 0, refY = 0;
    double residual = 0;
};

// ── 几何校正 UI 状态 ──
struct GeometricInput
{
    // 影像路径
    QString sourceImage;
    QString referenceImage;

    // GCP 匹配
    QString matchingAlgorithm = "SIFT";   // "SIFT" / "SURF" / "NCC"
    QString matchingMode      = "Auto";   // "Auto" / "SemiAuto" / "Manual"
    double  ratioThreshold    = 0.7;
    double  ransacThreshold   = 3.0;
    int     maxFeatures       = 5000;

    // 控制点列表
    QVector<GcpEntry> gcps;

    // 校正模型
    QString modelType        = "Polynomial2";
    int     polynomialOrder  = 2;
    QString resampleMethod   = "Bilinear";

    // 输出
    QString outputPath;
    QString outputProjection;
    double  outputPixelSizeX = 0;
    double  outputPixelSizeY = 0;
    double  outputExtent[4]  = {0, 0, 0, 0};
    int     blockSize        = 512;
};

#endif // GEOMETRICTYPES_H
