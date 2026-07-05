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

    void release()
    {
        for (float* m : imageMasks) delete[] m;
        imageMasks.clear();
    }
};

class SeamlineGenerator
{
public:
    /// 在重叠区域内生成 Voronoi 拼接线掩膜
    static SeamlineMask generateVoronoi(const QStringList& imagePaths,
                                         const double outputExtent[4],
                                         double resolution);

    /// 将 Voronoi 掩膜导出为 ESRI Shapefile 多边形
    /// 每个源影像的有效区域为一个或多个多边形, 含 img_idx/img_path/img_name 属性
    /// 返回 true 表示导出成功
    static bool exportSeamlinesToShapefile(const SeamlineMask& mask,
                                           const QStringList& imagePaths,
                                           const QString& shpPath);
};

#endif // SEAMLINEGENERATOR_H
