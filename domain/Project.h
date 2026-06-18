#ifndef PROJECT_H
#define PROJECT_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QMetaType>
#include "domain/ImageSource.h"
#include "domain/PipelineDefinition.h"

struct OutputConfig
{
    QString outputDirectory;        // 输出目录
    QString outputFormat = "GeoTIFF";  // "GeoTIFF" / "ENVI"
    QString namingPattern;          // 命名模板, 空则自动生成
    bool cleanupIntermediates = false;  // 完成后是否清理中间文件
    bool autoConfirm = true;            // 各阶段处理完不弹确认框
};

struct Project
{
    QString projectName;
    QString projectPath;                // .rjp 文件路径 (保存后填充)
    QString description;

    QList<ImageSource> imageSources;
    PipelineDefinition pipeline;
    OutputConfig output;

    bool isValid() const
    {
        return !projectName.isEmpty()
            && !imageSources.isEmpty()
            && pipeline.isValid();
    }

    bool hasRole(ImageRole role) const
    {
        for (const auto& src : imageSources)
        {
            if (src.role == role)
                return true;
        }
        return false;
    }

    QStringList mosaicInputPaths() const
    {
        QStringList paths;
        for (const auto& src : imageSources)
        {
            if (src.role == ImageRole::MosaicInput)
                paths.append(src.filePath);
        }
        return paths;
    }
};

Q_DECLARE_METATYPE(OutputConfig)
Q_DECLARE_METATYPE(Project)

#endif // PROJECT_H
