#include "WorkflowGraph.h"
#include <QMap>
#include <QSet>

QStringList WorkflowGraph::executionOrder() const
{
    QMap<QString, int> inDegree;
    QMap<QString, QList<QString>> adj;
    for (const auto& node : nodes)
    {
        inDegree[node.nodeId] = 0;
        adj[node.nodeId] = {};
    }
    for (const auto& edge : edges)
    {
        adj[edge.sourceNodeId].append(edge.targetNodeId);
        ++inDegree[edge.targetNodeId];
    }

    QStringList order;
    QStringList queue;
    for (auto it = inDegree.begin(); it != inDegree.end(); ++it)
    {
        if (it.value() == 0)
            queue.append(it.key());
    }

    while (!queue.isEmpty())
    {
        QString id = queue.takeFirst();
        order.append(id);
        for (const auto& neighbor : adj[id])
        {
            --inDegree[neighbor];
            if (inDegree[neighbor] == 0)
                queue.append(neighbor);
        }
    }
    return order;
}

bool WorkflowGraph::validate(QString& errorMessage) const
{
    if (nodes.isEmpty())
    {
        errorMessage = QStringLiteral("Workflow graph has no nodes");
        return false;
    }
    QSet<QString> nodeIds;
    for (const auto& node : nodes)
        nodeIds.insert(node.nodeId);
    for (const auto& edge : edges)
    {
        if (!nodeIds.contains(edge.sourceNodeId))
        {
            errorMessage = QStringLiteral("Edge references unknown source node: %1").arg(edge.sourceNodeId);
            return false;
        }
        if (!nodeIds.contains(edge.targetNodeId))
        {
            errorMessage = QStringLiteral("Edge references unknown target node: %1").arg(edge.targetNodeId);
            return false;
        }
    }
    return true;
}
