#ifndef ALGORITHMRESULT_H
#define ALGORITHMRESULT_H

#include <QString>

/**
 * @brief 算法执行结果结构体
 *
 * 所有算法类统一使用此结构体返回执行状态。
 * success 指示算法是否成功完成，
 * outputPath 存放输出文件路径（成功时有效），
 * errorMessage 存放错误描述（失败时有效）。
 */
struct AlgorithmResult
{
    /** @brief 算法是否成功执行 */
    bool success = false;
    /** @brief 输出文件路径（成功时有效） */
    QString outputPath;
    /** @brief 错误描述信息（失败时有效） */
    QString errorMessage;
};

#endif // ALGORITHMRESULT_H
