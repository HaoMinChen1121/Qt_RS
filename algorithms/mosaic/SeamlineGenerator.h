#ifndef SEAMLINEGENERATOR_H
#define SEAMLINEGENERATOR_H

#include <QString>
#include <QStringList>
#include <QVector>

struct SeamlineMask
{
    int   width  = 0;
    int   height = 0;
    double geoTransform[6] = {};
    QString projection;
    /// 对每景影像, 记录其在输出画布中负责的像素掩膜 (1=该影像负责)
    QVector<float*> imageMasks;  // size = imageCount, each [width*height]
};

class SeamlineGenerator
{
public:
    /// 在重叠区域内生成 Voronoi 拼接线掩膜
    /// imagePaths: 输入影像列表
    /// outputExtent: {xMin, yMin, xMax, yMax} — 输出范围 (留空=自动从输入推导)
    /// resolution: 输出分辨率 (米/像素)
    static SeamlineMask generateVoronoi(const QStringList& imagePaths,
                                         const double outputExtent[4],
                                         double resolution);
};

#endif // SEAMLINEGENERATOR_H
