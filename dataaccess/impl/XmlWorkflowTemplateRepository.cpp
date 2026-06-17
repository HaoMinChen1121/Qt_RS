#include "XmlWorkflowTemplateRepository.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>

bool XmlWorkflowTemplateRepository::save(const WorkflowGraph& graph, const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "[XmlWorkflowTemplateRepository] cannot write:" << filePath;
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("WorkflowGraph"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
    xml.writeTextElement(QStringLiteral("Name"), graph.graphName);
    xml.writeTextElement(QStringLiteral("Description"), graph.description);

    xml.writeStartElement(QStringLiteral("Nodes"));
    for (const auto& node : graph.nodes)
    {
        xml.writeStartElement(QStringLiteral("Node"));
        xml.writeAttribute(QStringLiteral("id"), node.nodeId);
        xml.writeAttribute(QStringLiteral("type"), node.nodeType);
        xml.writeAttribute(QStringLiteral("displayName"), node.displayName);
        xml.writeAttribute(QStringLiteral("posX"), QString::number(node.posX, 'f', 2));
        xml.writeAttribute(QStringLiteral("posY"), QString::number(node.posY, 'f', 2));

        for (auto it = node.properties.begin(); it != node.properties.end(); ++it)
        {
            xml.writeStartElement(QStringLiteral("Property"));
            xml.writeAttribute(QStringLiteral("key"), it.key());
            xml.writeAttribute(QStringLiteral("value"), it.value().toString());
            xml.writeEndElement(); // Property
        }

        xml.writeEndElement(); // Node
    }
    xml.writeEndElement(); // Nodes

    xml.writeStartElement(QStringLiteral("Edges"));
    for (const auto& edge : graph.edges)
    {
        xml.writeStartElement(QStringLiteral("Edge"));
        xml.writeAttribute(QStringLiteral("id"), edge.edgeId);
        xml.writeAttribute(QStringLiteral("source"), edge.sourceNodeId);
        xml.writeAttribute(QStringLiteral("sourcePort"), edge.sourcePort);
        xml.writeAttribute(QStringLiteral("target"), edge.targetNodeId);
        xml.writeAttribute(QStringLiteral("targetPort"), edge.targetPort);
        xml.writeEndElement(); // Edge
    }
    xml.writeEndElement(); // Edges

    xml.writeEndElement(); // WorkflowGraph
    xml.writeEndDocument();
    file.close();

    qDebug() << "[XmlWorkflowTemplateRepository] saved:" << filePath
             << "nodes:" << graph.nodes.size() << "edges:" << graph.edges.size();
    return true;
}

WorkflowGraph XmlWorkflowTemplateRepository::load(const QString& filePath)
{
    WorkflowGraph graph;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "[XmlWorkflowTemplateRepository] cannot read:" << filePath;
        return graph;
    }

    QXmlStreamReader xml(&file);

    // Current parsing context
    enum Ctx { Root, Nodes, Edges, Node, Edge } ctx = Root;
    WorkflowNode currentNode;
    WorkflowEdge currentEdge;

    while (!xml.atEnd() && !xml.hasError())
    {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token != QXmlStreamReader::StartElement)
            continue;

        if (xml.name() == QStringLiteral("WorkflowGraph"))
        {
            ctx = Root;
        } else if (xml.name() == QStringLiteral("Name"))
        {
            graph.graphName = xml.readElementText();
        } else if (xml.name() == QStringLiteral("Description"))
        {
            graph.description = xml.readElementText();
        } else if (xml.name() == QStringLiteral("Nodes"))
        {
            ctx = Nodes;
        } else if (xml.name() == QStringLiteral("Node"))
        {
            currentNode = WorkflowNode{};
            currentNode.nodeId = xml.attributes().value("id").toString();
            currentNode.nodeType = xml.attributes().value("type").toString();
            currentNode.displayName = xml.attributes().value("displayName").toString();
            currentNode.posX = xml.attributes().value("posX").toDouble();
            currentNode.posY = xml.attributes().value("posY").toDouble();
            ctx = Node;
        } else if (xml.name() == QStringLiteral("Property"))
        {
            if (ctx == Node)
            {
                QString key = xml.attributes().value("key").toString();
                QString val = xml.attributes().value("value").toString();
                currentNode.properties.insert(key, QVariant(val));
            }
        } else if (xml.name() == QStringLiteral("Edges"))
        {
            // finalize any pending node
            if (ctx == Node)
                graph.nodes.append(currentNode);
            ctx = Edges;
        } else if (xml.name() == QStringLiteral("Edge"))
        {
            currentEdge = WorkflowEdge{};
            currentEdge.edgeId = xml.attributes().value("id").toString();
            currentEdge.sourceNodeId = xml.attributes().value("source").toString();
            currentEdge.sourcePort = xml.attributes().value("sourcePort").toString();
            currentEdge.targetNodeId = xml.attributes().value("target").toString();
            currentEdge.targetPort = xml.attributes().value("targetPort").toString();
            graph.edges.append(currentEdge);
            ctx = Edge;
        }
    }

    // Finalize last node if we were still parsing one
    if (ctx == Node)
        graph.nodes.append(currentNode);

    file.close();

    if (xml.hasError())
    {
        qWarning() << "[XmlWorkflowTemplateRepository] XML error:" << xml.errorString();
    }

    qDebug() << "[XmlWorkflowTemplateRepository] loaded:" << filePath
             << "nodes:" << graph.nodes.size() << "edges:" << graph.edges.size();
    return graph;
}
