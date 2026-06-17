#ifndef DNTOREFLECTANCE_H
#define DNTOREFLECTANCE_H

#include "algorithms/common/AlgorithmResult.h"
#include "algorithms/common/ProgressCallback.h"
#include "domain/params/RadiometricCorrectionParams.h"
#include "domain/SensorInfo.h"

class IRasterReader;
class IRasterWriter;

namespace Radiometric
{

/**
 * @brief DN值 → 大气顶层（TOA）反射率转换算法
 *
 * 公式：ρ = π × L × d² / (Esun × cos(θ_solar))
 *
 * 其中：
 *   L      = gain × DN + offset （先做辐亮度转换）
 *   d      = 日地距离（天文单位 AU）
 *   Esun   = 大气顶层太阳光谱辐照度（W/m²/μm）
 *   θ_solar = 太阳天顶角
 *
 * 优先使用传感器元数据中的太阳天顶角和日地距离；
 * 若元数据缺失，则使用 RadiometricCorrectionParams 中用户指定的值。
 * 使用分块（512×512）处理以避免大影像的内存溢出。
 */
class DnToReflectance
{
public:
    DnToReflectance() = default;

    /**
     * @brief 执行 DN → TOA 反射率转换
     * @param reader  已打开源影像的读取器
     * @param writer  用于写入输出影像的写入器
     * @param params  辐射定标参数
     * @param sensorInfo 传感器元数据（太阳天顶角、日地距离、各波段 Esun）
     * @param progress 进度回调，返回 false 则取消
     * @return         AlgorithmResult
     */
    AlgorithmResult process(IRasterReader* reader,
                            IRasterWriter* writer,
                            const RadiometricCorrectionParams& params,
                            const SensorInfo& sensorInfo,
                            ProgressCallback progress = nullptr,
                            int currentBand = 1);
};

} // namespace Radiometric

#endif // DNTOREFLECTANCE_H
