#include "FeatheringBlender.h"
#include <algorithm>
#include <cmath>

float* FeatheringBlender::computeWeights(const float* mask, int w, int h,
                                          int featherWidth)
                                          {
    int pixels = w * h;
    float* dist  = new float[pixels];
    float* weight = new float[pixels];

    // 距离变换: 对掩膜内(>0)的距离=0, 掩膜外的用 3x3 邻域近似
    const float INF = 1e10f;
    for (int i = 0; i < pixels; ++i)
        dist[i] = (mask[i] > 0.5f) ? 0.0f : INF;

    // 正向扫描
    for (int y = 1; y < h; ++y)
        for (int x = 1; x < w; ++x)
        {
            int idx = y * w + x;
            float d = std::min({dist[idx],
                dist[idx - 1] + 1.0f, dist[idx - w] + 1.0f,
                dist[idx - w - 1] + 1.414f});
            dist[idx] = d;
        }
    // 反向扫描
    for (int y = h - 2; y >= 0; --y)
        for (int x = w - 2; x >= 0; --x)
        {
            int idx = y * w + x;
            float d = std::min({dist[idx],
                dist[idx + 1] + 1.0f, dist[idx + w] + 1.0f,
                dist[idx + w + 1] + 1.414f});
            dist[idx] = d;
        }

    // 距离 → 线性羽化权重
    float fw = (float)std::max(1, featherWidth);
    for (int i = 0; i < pixels; ++i)
    {
        if (mask[i] > 0.5f)
            weight[i] = std::max(0.0f, 1.0f - dist[i] / fw);
        else
            weight[i] = std::min(1.0f, dist[i] / fw);
    }

    delete[] dist;
    return weight;
}
