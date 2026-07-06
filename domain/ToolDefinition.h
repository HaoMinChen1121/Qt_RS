#ifndef TOOLDEFINITION_H
#define TOOLDEFINITION_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QList>
#include <QMetaType>

struct ToolParameter {
    QString name;
    QString label;
    QString type;            // "file" | "string" | "int" | "double" | "choice" | "crs" | "folder"
    QVariant defaultValue;
    QStringList choices;
    QString fileFilter;
};

struct ToolDefinition {
    QString toolId;
    QString displayName;
    QString category;
    QString description;
    QList<ToolParameter> parameters;
};

Q_DECLARE_METATYPE(ToolDefinition)

#endif // TOOLDEFINITION_H
