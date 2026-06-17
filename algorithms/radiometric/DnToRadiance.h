#ifndef DNTORADIANCE_H
#define DNTORADIANCE_H

#include "algorithms/common/AlgorithmResult.h"
#include "algorithms/common/ProgressCallback.h"
#include "domain/params/RadiometricCorrectionParams.h"
#include "domain/SensorInfo.h"

class IRasterReader;
class IRasterWriter;

namespace Radiometric
{

/**
 * @brief DN值 → 辐亮度转换算法
 *
 * 公式：L = gain × DN + offset
 *
 * 使用分块（512×512）读取源影像，逐波段逐块计算辐亮度后写入输出文件。
 * 增益（gain）和偏置（offset）可自动从传感器元数据中提取，也可手动指定。
 * 输出文件保留与源影像相同的地理参考和投影信息。
 */
class DnToRadiance
{
public:
    DnToRadiance() = default;

    /**
     * @brief 执行 DN → 辐亮度转换
     * @param reader  已打开源影像的读取器（仅读取一个文件，无需 reopen）
     * @param writer  用于写入输出影像的写入器
     * @param params  辐射定标参数（指定自动/手动增益偏置、输出路径等）
     * @param sensorInfo 传感器元数据（包含各波段增益、偏置）
     * @param progress 进度回调，每处理完一个分块调用一次；返回 false 则取消
     * @return         AlgorithmResult，success 为 true 时 outputPath 有效
     */
    AlgorithmResult process(IRasterReader* reader,
                            IRasterWriter* writer,
                            const RadiometricCorrectionParams& params,
                            const SensorInfo& sensorInfo,
                            ProgressCallback progress = nullptr,
                            int currentBand = 1);
};

} // namespace Radiometric

#endif // DNTORADIANCE_H
