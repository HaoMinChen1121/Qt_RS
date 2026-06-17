#ifndef FEATHERINGBLENDER_H
#define FEATHERINGBLENDER_H

#include <QString>
#include <QVector>

/// 线性羽化融合：沿拼接线缓冲区逐像素混合
class FeatheringBlender
{
public:
    /// 对掩膜做距离变换 → 羽化权重
    /// mask: 输入二值掩膜 (1=该影像负责, 0=其他), width x height
    /// featherWidth: 羽化宽度 (像素)
    /// 输出: weight [0,1]，掩膜内为 1，边界外 featherWidth 范围内线性衰减到 0
    static float* computeWeights(const float* mask, int width, int height,
                                  int featherWidth);
};

#endif // FEATHERINGBLENDER_H
