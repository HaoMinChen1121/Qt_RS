#ifndef ATMOSPHERICCORRECTOR_H
#define ATMOSPHERICCORRECTOR_H

#include <QVector>
#include "algorithms/common/AlgorithmResult.h"
#include "algorithms/common/ProgressCallback.h"
#include "domain/params/RadiometricCorrectionParams.h"
#include "domain/SensorInfo.h"

class IRasterReader;
class IRasterWriter;

namespace Radiometric
{

/**
 * @brief 单个波段的大气校正系数（6S/Py6S输出）
 *
 * 大气校正公式（Lambertian假设）：
 *   y  = xa * ρ_toa - xb
 *   ρ_surface = y / (1.0 + xc * y)
 *
 * 其中 ρ_toa 为大气顶层表观反射率，xa/xb分别为透过率倒数与大气程辐射，
 * xc为球面反照率。
 */
struct AtmosphericCoefficients
{
    int    bandIndex      = 0;   // 1-based，对应 sensorInfo.bands 顺序
    double wavelengthMin  = 0.0; // µm
    double wavelengthMax  = 0.0;
    double xa             = 1.0; // 大气透过率倒数
    double xb             = 0.0; // 大气程辐射（反射率量纲）
    double xc             = 0.0; // 球面反照率
};

/**
 * @brief 大气校正编排器
 *
 * 支持三种大气校正模型：
 *   - "6S"：  通过QProcess调用sixsV2.1可执行文件，解析输出得到逐波段校正系数，
 *             然后对TOA反射率影像逐像素反演地表反射率
 *   - "Py6S"：生成完整Py6S脚本，通过QProcess调用python执行，解析JSON系数，
 *             然后对TOA反射率影像逐像素反演地表反射率
 *   - "Sen2Cor"：通过QProcess调用ESA的L2A_Process，由Sen2Cor自行完成全部处理
 *   - "None"：跳过大气校正（调用方应回退到TOA反射率作为中间产物）
 *
 * 典型调用流程（6S/Py6S）：
 *   1. coefs = computeCorrectionCoefficients(params, sensorInfo, workDir, progress)
 *       — 生成输入 → 运行模型 → 解析系数（一次性处理所有波段）
 *   2. applyCorrection(reader, writer, coefs[n], progress)
 *       — 逐波段对TOA反射率影像做像素级反演
 *
 * 典型调用流程（Sen2Cor）：
 *   1. runSen2Cor(l1cProductDir, params, workDir)
 *       — Sen2Cor自行完成全部处理，输出L2A产品目录
 */
class AtmosphericCorrector
{
public:
    AtmosphericCorrector() = default;

    /**
     * @brief 运行大气模型并获取逐波段校正系数（6S / Py6S）
     * @param params      辐射定标参数（含大气模型选择、气溶胶参数等）
     * @param sensorInfo  传感器元数据（太阳/传感器角度、各波段波长范围）
     * @param workingDir  工作目录（用于存放生成的中间文件）
     * @param progress    进度回调
     * @return            逐波段大气校正系数列表（按sensorInfo.bands顺序）
     */
    QVector<AtmosphericCoefficients> computeCorrectionCoefficients(
        const RadiometricCorrectionParams& params,
        const SensorInfo& sensorInfo,
        const QString& workingDir,
        ProgressCallback progress = nullptr);

    /**
     * @brief 对单个波段的TOA反射率逐像素应用大气校正
     * @param reader    已打开的TOA反射率影像
     * @param writer    已创建的输出影像写入器
     * @param bandIndex 要处理的波段索引（1-based GDAL band index）
     * @param coef      该波段对应的大气校正系数
     * @param progress  进度回调
     * @return          AlgorithmResult
     */
    AlgorithmResult applyCorrection(IRasterReader* reader,
                                    IRasterWriter* writer,
                                    int bandIndex,
                                    const AtmosphericCoefficients& coef,
                                    ProgressCallback progress = nullptr);

    /**
     * @brief 运行 Sen2Cor 大气校正
     *
     * 针对 Sentinel-2 L1C 产品目录，调用 ESA Sen2Cor 生成 L2A 产品。
     * 注意：此方法会尝试从输入路径定位 .SAFE 根目录。
     *
     * @param inputPath   L1C 产品目录或其中的波段文件路径
     * @param params      辐射定标参数（含分辨率等Sen2Cor选项）
     * @param workingDir  工作目录
     * @return            AlgorithmResult（成功时 outputPath 为 L2A 目录）
     */
    AlgorithmResult runSen2Cor(const QString& inputPath,
                               const RadiometricCorrectionParams& params,
                               const QString& workingDir,
                               ProgressCallback progress = nullptr);

private:
    /// 6S / Py6S 输入生成
    QByteArray generate6sInputData(const RadiometricCorrectionParams& params,
                                    const SensorInfo& sensorInfo,
                                    double wlMin, double wlMax);

    QString generate6sInputFile(const RadiometricCorrectionParams& params,
                                const SensorInfo& sensorInfo,
                                const QString& workingDir);

    QString generatePy6sScript(const RadiometricCorrectionParams& params,
                               const SensorInfo& sensorInfo,
                               const QString& workingDir);

    /// 外部进程调用
    QString run6sProcess(const QByteArray& inputData,
                         const QString& workingDir);

    QString run6sProcess(const QString& inputFilePath,
                         const QString& workingDir);

    QString runPy6sProcess(const QString& scriptPath,
                           const QString& workingDir);

    /// 输出解析
    QVector<AtmosphericCoefficients> parse6sOutput(const QString& stdoutText,
                                                     const SensorInfo& sensorInfo,
                                                     bool isRadianceMode);

    QVector<AtmosphericCoefficients> parsePy6sOutput(const QString& jsonText);
};

} // namespace Radiometric

#endif // ATMOSPHERICCORRECTOR_H
