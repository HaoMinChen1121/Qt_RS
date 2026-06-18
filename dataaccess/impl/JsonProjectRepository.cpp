#include "JsonProjectRepository.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

static const char* kVersion = "1.0";

// ── 辅助: QVariantMap → QJsonObject ──
static QJsonObject variantMapToJson(const QVariantMap& map)
{
    QJsonObject obj;
    for (auto it = map.begin(); it != map.end(); ++it)
        obj[it.key()] = QJsonValue::fromVariant(it.value());
    return obj;
}

static QVariantMap jsonToVariantMap(const QJsonObject& obj)
{
    QVariantMap map;
    for (auto it = obj.begin(); it != obj.end(); ++it)
        map[it.key()] = it.value().toVariant();
    return map;
}

// ── 辅助: ImageRole 字符串转换 ──
static QString roleToString(ImageRole role)
{
    switch (role)
    {
    case ImageRole::MosaicInput:  return QStringLiteral("MosaicInput");
    case ImageRole::Panchromatic: return QStringLiteral("Panchromatic");
    case ImageRole::Reference:    return QStringLiteral("Reference");
    case ImageRole::HistReference:return QStringLiteral("HistReference");
    }
    return QStringLiteral("MosaicInput");
}

static ImageRole stringToRole(const QString& s)
{
    if (s == QStringLiteral("Panchromatic")) return ImageRole::Panchromatic;
    if (s == QStringLiteral("Reference"))    return ImageRole::Reference;
    if (s == QStringLiteral("HistReference"))return ImageRole::HistReference;
    return ImageRole::MosaicInput;
}

// ── 辅助: StageScope 字符串转换 ──
static QString scopeToString(StageScope s)
{
    return (s == StageScope::AllImages) ? QStringLiteral("AllImages")
                                        : QStringLiteral("PerImage");
}

static StageScope stringToScope(const QString& s)
{
    return (s == QStringLiteral("AllImages")) ? StageScope::AllImages
                                              : StageScope::PerImage;
}

// ────────────────────────────────────────────────────────────
// 序列化
// ────────────────────────────────────────────────────────────
bool JsonProjectRepository::save(const Project& project, const QString& filePath)
{
    QJsonObject root;
    root[QStringLiteral("version")]     = kVersion;
    root[QStringLiteral("projectName")] = project.projectName;
    root[QStringLiteral("description")] = project.description;

    // ImageSources
    QJsonArray srcArr;
    for (const auto& src : project.imageSources)
    {
        QJsonObject s;
        s[QStringLiteral("sourceId")]    = src.sourceId;
        s[QStringLiteral("displayName")] = src.displayName;
        s[QStringLiteral("filePath")]    = src.filePath;
        s[QStringLiteral("sensorType")]  = src.sensorType;
        s[QStringLiteral("role")]        = roleToString(src.role);

        QJsonArray bandArr;
        for (const auto& bs : src.bandSelections)
        {
            QJsonObject b;
            b[QStringLiteral("purpose")] = bs.purpose;
            QJsonArray nums;
            for (int n : bs.bandNumbers) nums.append(n);
            b[QStringLiteral("bandNumbers")] = nums;
            bandArr.append(b);
        }
        s[QStringLiteral("bandSelections")] = bandArr;
        s[QStringLiteral("metadata")] = variantMapToJson(src.metadata);

        srcArr.append(s);
    }
    root[QStringLiteral("imageSources")] = srcArr;

    // Pipeline
    QJsonObject pipe;
    pipe[QStringLiteral("name")]        = project.pipeline.name;
    pipe[QStringLiteral("description")] = project.pipeline.description;

    QJsonArray stagesArr;
    for (const auto& stage : project.pipeline.stages)
    {
        QJsonObject st;
        st[QStringLiteral("stageId")]    = stage.stageId;
        st[QStringLiteral("displayName")] = stage.displayName;
        st[QStringLiteral("scope")]      = scopeToString(stage.scope);
        st[QStringLiteral("params")]     = variantMapToJson(stage.params);
        st[QStringLiteral("enabled")]    = stage.enabled;
        st[QStringLiteral("required")]   = stage.required;
        stagesArr.append(st);
    }
    pipe[QStringLiteral("stages")] = stagesArr;
    root[QStringLiteral("pipeline")] = pipe;

    // Output
    QJsonObject out;
    out[QStringLiteral("outputDirectory")]       = project.output.outputDirectory;
    out[QStringLiteral("outputFormat")]          = project.output.outputFormat;
    out[QStringLiteral("namingPattern")]         = project.output.namingPattern;
    out[QStringLiteral("cleanupIntermediates")]  = project.output.cleanupIntermediates;
    out[QStringLiteral("autoConfirm")]           = project.output.autoConfirm;
    root[QStringLiteral("output")] = out;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "[JsonProjectRepository] cannot write:" << filePath;
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    qDebug() << "[JsonProjectRepository] saved:" << filePath;
    return true;
}

// ────────────────────────────────────────────────────────────
// 反序列化
// ────────────────────────────────────────────────────────────
Project JsonProjectRepository::load(const QString& filePath)
{
    Project project;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "[JsonProjectRepository] cannot read:" << filePath;
        return project;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject())
    {
        qWarning() << "[JsonProjectRepository] invalid JSON:" << filePath;
        return project;
    }

    QJsonObject root = doc.object();
    project.projectName = root.value(QStringLiteral("projectName")).toString();
    project.description = root.value(QStringLiteral("description")).toString();
    project.projectPath = filePath;

    // ImageSources
    QJsonArray srcArr = root.value(QStringLiteral("imageSources")).toArray();
    for (const auto& val : srcArr)
    {
        QJsonObject s = val.toObject();
        ImageSource src;
        src.sourceId    = s.value(QStringLiteral("sourceId")).toString();
        src.displayName = s.value(QStringLiteral("displayName")).toString();
        src.filePath    = s.value(QStringLiteral("filePath")).toString();
        src.sensorType  = s.value(QStringLiteral("sensorType")).toString();
        src.role        = stringToRole(s.value(QStringLiteral("role")).toString());

        QJsonArray bandArr = s.value(QStringLiteral("bandSelections")).toArray();
        for (const auto& bval : bandArr)
        {
            QJsonObject b = bval.toObject();
            BandSelection bs;
            bs.purpose = b.value(QStringLiteral("purpose")).toString();
            QJsonArray nums = b.value(QStringLiteral("bandNumbers")).toArray();
            for (const auto& n : nums)
                bs.bandNumbers.append(n.toInt());
            src.bandSelections.append(bs);
        }

        src.metadata = jsonToVariantMap(s.value(QStringLiteral("metadata")).toObject());
        project.imageSources.append(src);
    }

    // Pipeline
    QJsonObject pipe = root.value(QStringLiteral("pipeline")).toObject();
    project.pipeline.name        = pipe.value(QStringLiteral("name")).toString();
    project.pipeline.description = pipe.value(QStringLiteral("description")).toString();

    QJsonArray stagesArr = pipe.value(QStringLiteral("stages")).toArray();
    for (const auto& val : stagesArr)
    {
        QJsonObject st = val.toObject();
        PipelineStage stage;
        stage.stageId     = st.value(QStringLiteral("stageId")).toString();
        stage.displayName = st.value(QStringLiteral("displayName")).toString();
        stage.scope       = stringToScope(st.value(QStringLiteral("scope")).toString());
        stage.params      = jsonToVariantMap(st.value(QStringLiteral("params")).toObject());
        stage.enabled     = st.value(QStringLiteral("enabled")).toBool(true);
        stage.required    = st.value(QStringLiteral("required")).toBool(false);
        project.pipeline.stages.append(stage);
    }

    // Output
    QJsonObject out = root.value(QStringLiteral("output")).toObject();
    project.output.outputDirectory       = out.value(QStringLiteral("outputDirectory")).toString();
    project.output.outputFormat = out.contains(QStringLiteral("outputFormat"))
        ? out.value(QStringLiteral("outputFormat")).toString() : QStringLiteral("GeoTIFF");
    project.output.namingPattern = out.value(QStringLiteral("namingPattern")).toString();
    project.output.cleanupIntermediates = out.contains(QStringLiteral("cleanupIntermediates"))
        ? out.value(QStringLiteral("cleanupIntermediates")).toBool() : false;
    project.output.autoConfirm = out.contains(QStringLiteral("autoConfirm"))
        ? out.value(QStringLiteral("autoConfirm")).toBool() : true;

    qDebug() << "[JsonProjectRepository] loaded:" << filePath
             << "sources:" << project.imageSources.size()
             << "stages:" << project.pipeline.stages.size();
    return project;
}
