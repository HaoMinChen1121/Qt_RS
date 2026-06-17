#ifndef WORKFLOWGRAPH_H
#define WORKFLOWGRAPH_H

#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QMetaType>

struct WorkflowNode
{
    QString nodeId;
    QString nodeType;   // "Read", "Calibrate", "AtmCorrect", "Clip", "Resample", "Fusion", "Mosaic", "Write"
    QString displayName;
    QMap<QString, QVariant> properties;
    double posX = 0.0;
    double posY = 0.0;
};

struct WorkflowEdge
{
    QString edgeId;
    QString sourceNodeId;
    QString sourcePort;
    QString targetNodeId;
    QString targetPort;
};

struct WorkflowGraph
{
    QString graphName;
    QString description;
    QList<WorkflowNode> nodes;
    QList<WorkflowEdge> edges;

    QStringList executionOrder() const;
    bool validate(QString& errorMessage) const;
};

Q_DECLARE_METATYPE(WorkflowGraph)

#endif // WORKFLOWGRAPH_H
