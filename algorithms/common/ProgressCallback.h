#ifndef PROGRESSCALLBACK_H
#define PROGRESSCALLBACK_H

#include <functional>

class QString;

/**
 * @brief 进度回调函数类型
 *
 * 参数：
 *   - percent:      当前进度（0 ~ 100）
 *   - statusMessage: 当前状态描述文本
 * 返回值：
 *   - true  → 继续处理
 *   - false → 请求取消（算法应在检查到 false 后尽快返回）
 *
 * 所有算法类通过此回调向调用者报告进度，
 * 调用者（如 TaskWorker）可在回调中检查取消标志并返回 false。
 */
using ProgressCallback = std::function<bool(int percent, const QString& statusMessage)>;

#endif // PROGRESSCALLBACK_H
