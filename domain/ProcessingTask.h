#ifndef PROCESSINGTASK_H
#define PROCESSINGTASK_H

#include <QString>
#include <QVariantMap>
#include <QMetaType>

/**
 * @brief 批处理任务实体
 *
 * 由 IBatchService 管理生命周期，状态枚举与 BatchProcessPanel::TaskStatus 对齐。
 */
struct ProcessingTask
{
    int taskId = -1;
    QString taskName;
    QString taskType;   // "Radiometric", "Geometric", "Fusion", "Mosaic"
    int status = 0;     // 0=Pending, 1=Running, 2=Paused, 3=Completed, 4=Failed, 5=Cancelled
    int progress = 0;   // 0..100
    double elapsedSeconds = 0.0;
    QVariantMap parameters;  // stores the actual param struct as a variant
    QString outputPath;
    QString errorMessage;
};

Q_DECLARE_METATYPE(ProcessingTask)

#endif // PROCESSINGTASK_H
