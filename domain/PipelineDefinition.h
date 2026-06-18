#ifndef PIPELINEDEFINITION_H
#define PIPELINEDEFINITION_H

#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QMetaType>

enum class StageScope
{
    PerImage,     // 逐景执行 — 对每个输入影像独立运行
    AllImages     // 全量执行 — 所有影像一起处理（如镶嵌）
};

struct PipelineStage
{
    QString stageId;                    // "Read", "Calibrate", "AtmCorrect", "Geometric",
                                        // "Clip", "Resample", "Fusion", "Mosaic", "Write"
    QString displayName;                // 显示名: "辐射定标"
    StageScope scope = StageScope::PerImage;
    QMap<QString, QVariant> params;     // 阶段参数
    bool enabled = true;                // 用户可关闭
    bool required = false;              // 是否必须阶段 (Read/Write 不可跳过)
};

struct PipelineDefinition
{
    QString name;                       // "标准全流程"
    QString description;
    QList<PipelineStage> stages;

    QStringList enabledStageIds() const
    {
        QStringList ids;
        for (const auto& s : stages)
        {
            if (s.enabled)
                ids.append(s.stageId);
        }
        return ids;
    }

    bool isValid() const { return !stages.isEmpty(); }
};

Q_DECLARE_METATYPE(PipelineStage)
Q_DECLARE_METATYPE(PipelineDefinition)

#endif // PIPELINEDEFINITION_H
