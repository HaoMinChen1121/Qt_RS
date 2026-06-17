#ifndef RADIOMETRICWORKER_H
#define RADIOMETRICWORKER_H

#include "controllers/TaskWorker.h"
#include "domain/params/RadiometricCorrectionParams.h"
#include "domain/SensorInfo.h"

class IRasterReader;
class IRasterWriter;

/**
 * @brief 辐射定标异步工作单元
 *
 * TaskWorker 的子类，负责在子线程中执行辐射定标与大气校正算法。
 * process() 槽中的处理流程：
 *
 *   Sen2Cor 模式（全目录级处理）：
 *     - 直接调用 Sen2Cor 对 L1C 产品目录进行大气校正
 *     - Sen2Cor 内部完成 DN→L2A 的全部处理，无需逐波段操作
 *
 *   6S / Py6S 模式（两步流程）：
 *     Phase 1 — 运行大气模型获取全波段校正系数（computeCorrectionCoefficients）
 *     Phase 2 — 逐文件：
 *       1. DnToRadiance 或 DnToReflectance 生成 TOA 中间产物
 *       2. 重新打开 TOA 文件，逐像素应用大气校正（applyCorrection）
 *       3. 输出地表反射率影像
 *
 *   None 模式（无大气校正）：
 *     - 仅执行 DnToRadiance 或 DnToReflectance，TOA 产物即为最终输出
 *
 * 通过 moveToThread() + QThread 实现非阻塞处理，
 * 通过 isCancelled() 合作式取消（每处理完一个分块检查一次）。
 */
class RadiometricWorker : public TaskWorker
{
    Q_OBJECT
public:
    /**
     * @param reader      影像读取器（由 Service 注入，指针归 Service 所有）
     * @param writer      影像写入器（由 Service 注入，指针归 Service 所有）
     * @param params      辐射定标参数
     * @param sensorInfo  传感器元数据（由 Service 在构造前解析）
     * @param parent      父 QObject
     */
    RadiometricWorker(IRasterReader* reader,
                      IRasterWriter* writer,
                      const RadiometricCorrectionParams& params,
                      const SensorInfo& sensorInfo,
                      QObject* parent = nullptr);

public slots:
    /** @brief 在子线程中调用，执行实际的辐射定标与大气校正算法 */
    void process() override;

private:
    IRasterReader* mReader;
    IRasterWriter* mWriter;
    RadiometricCorrectionParams mParams;
    SensorInfo mSensorInfo;
};

#endif // RADIOMETRICWORKER_H
