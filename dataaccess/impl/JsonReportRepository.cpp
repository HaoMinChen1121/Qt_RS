#include "JsonReportRepository.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QTextStream>

bool JsonReportRepository::save(const ProcessingReport& report, const QString& filePath)
{
    QJsonObject root;
    root.insert(QStringLiteral("reportTitle"), report.reportTitle);
    root.insert(QStringLiteral("generatedAt"), report.generatedAt.toString(Qt::ISODate));
    root.insert(QStringLiteral("totalTasks"), report.totalTasks);
    root.insert(QStringLiteral("successCount"), report.successCount);
    root.insert(QStringLiteral("failureCount"), report.failureCount);
    root.insert(QStringLiteral("totalDurationSeconds"), report.totalDurationSeconds);

    QJsonArray stepsArr;
    for (const auto& step : report.steps)
    {
        QJsonObject so;
        so.insert(QStringLiteral("stepName"), step.stepName);
        so.insert(QStringLiteral("status"), step.status);
        so.insert(QStringLiteral("durationSeconds"), step.durationSeconds);
        so.insert(QStringLiteral("outputPath"), step.outputPath);
        so.insert(QStringLiteral("errorMessage"), step.errorMessage);

        QJsonObject paramsObj;
        for (auto it = step.parameters.begin(); it != step.parameters.end(); ++it)
            paramsObj.insert(it.key(), it.value());
        so.insert(QStringLiteral("parameters"), paramsObj);

        stepsArr.append(so);
    }
    root.insert(QStringLiteral("steps"), stepsArr);

    QJsonArray tasksArr;
    for (const auto& t : report.taskSummaries)
    {
        QJsonObject to;
        to.insert(QStringLiteral("taskId"), t.taskId);
        to.insert(QStringLiteral("taskName"), t.taskName);
        to.insert(QStringLiteral("taskType"), t.taskType);
        to.insert(QStringLiteral("status"), t.status);
        to.insert(QStringLiteral("progress"), t.progress);
        to.insert(QStringLiteral("elapsedSeconds"), t.elapsedSeconds);
        to.insert(QStringLiteral("outputPath"), t.outputPath);
        to.insert(QStringLiteral("errorMessage"), t.errorMessage);
        tasksArr.append(to);
    }
    root.insert(QStringLiteral("taskSummaries"), tasksArr);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "[JsonReportRepository] cannot write:" << filePath;
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "[JsonReportRepository] saved:" << filePath << "tasks:" << report.totalTasks;
    return true;
}

ProcessingReport JsonReportRepository::load(const QString& filePath)
{
    ProcessingReport report;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "[JsonReportRepository] cannot read:" << filePath;
        return report;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject())
    {
        qWarning() << "[JsonReportRepository] invalid JSON in:" << filePath;
        return report;
    }

    QJsonObject root = doc.object();
    report.reportTitle = root.value("reportTitle").toString();
    report.generatedAt = QDateTime::fromString(root.value("generatedAt").toString(), Qt::ISODate);
    report.totalTasks = root.value("totalTasks").toInt();
    report.successCount = root.value("successCount").toInt();
    report.failureCount = root.value("failureCount").toInt();
    report.totalDurationSeconds = root.value("totalDurationSeconds").toDouble();

    for (const auto& sv : root.value("steps").toArray())
    {
        QJsonObject so = sv.toObject();
        ProcessingStep step;
        step.stepName = so.value("stepName").toString();
        step.status = so.value("status").toString();
        step.durationSeconds = so.value("durationSeconds").toDouble();
        step.outputPath = so.value("outputPath").toString();
        step.errorMessage = so.value("errorMessage").toString();
        QJsonObject paramsObj = so.value("parameters").toObject();
        for (auto it = paramsObj.begin(); it != paramsObj.end(); ++it)
            step.parameters.insert(it.key(), it.value().toString());
        report.steps.append(step);
    }

    for (const auto& tv : root.value("taskSummaries").toArray())
    {
        QJsonObject to = tv.toObject();
        ProcessingTask task;
        task.taskId = to.value("taskId").toInt();
        task.taskName = to.value("taskName").toString();
        task.taskType = to.value("taskType").toString();
        task.status = to.value("status").toInt();
        task.progress = to.value("progress").toInt();
        task.elapsedSeconds = to.value("elapsedSeconds").toDouble();
        task.outputPath = to.value("outputPath").toString();
        task.errorMessage = to.value("errorMessage").toString();
        report.taskSummaries.append(task);
    }

    qDebug() << "[JsonReportRepository] loaded:" << filePath << "tasks:" << report.totalTasks;
    return report;
}

bool JsonReportRepository::exportHtml(const ProcessingReport& report, const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "[JsonReportRepository] cannot write HTML:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out << "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
        << "<title>" << report.reportTitle.toHtmlEscaped() << "</title>"
        << "<style>body{font-family:Arial,sans-serif;margin:20px}"
        << "table{border-collapse:collapse;width:100%}"
        << "th,td{border:1px solid #ccc;padding:6px;text-align:left}"
        << "th{background:#f0f0f0}.success{color:green}.failed{color:red}</style></head><body>";

    out << "<h1>" << report.reportTitle.toHtmlEscaped() << "</h1>"
        << "<p>Generated: " << report.generatedAt.toString(Qt::ISODate) << "</p>"
        << "<p>Total: " << report.totalTasks << " | Success: " << report.successCount
        << " | Failed: " << report.failureCount << " | Duration: "
        << report.totalDurationSeconds << "s</p>";

    if (!report.steps.isEmpty())
    {
        out << "<h2>Processing Steps</h2><table><tr><th>Step</th><th>Status</th>"
            << "<th>Duration (s)</th><th>Output</th><th>Error</th></tr>";
        for (const auto& s : report.steps)
        {
            QString css = (s.status == "Success") ? "success" :
                          (s.status == "Failed")  ? "failed" : "";
            out << "<tr><td>" << s.stepName.toHtmlEscaped() << "</td>"
                << "<td class=\"" << css << "\">" << s.status.toHtmlEscaped() << "</td>"
                << "<td>" << s.durationSeconds << "</td>"
                << "<td>" << s.outputPath.toHtmlEscaped() << "</td>"
                << "<td>" << s.errorMessage.toHtmlEscaped() << "</td></tr>";
        }
        out << "</table>";
    }

    if (!report.taskSummaries.isEmpty())
    {
        out << "<h2>Task Summaries</h2><table><tr><th>ID</th><th>Name</th><th>Type</th>"
            << "<th>Status</th><th>Progress</th><th>Time (s)</th><th>Output</th></tr>";
        for (const auto& t : report.taskSummaries)
        {
            static const char* statusNames[] = {"Pending","Running","Paused","Completed","Failed","Cancelled"};
            const char* stat = (t.status >= 0 && t.status < 6) ? statusNames[t.status] : "Unknown";
            out << "<tr><td>" << t.taskId << "</td>"
                << "<td>" << t.taskName.toHtmlEscaped() << "</td>"
                << "<td>" << t.taskType.toHtmlEscaped() << "</td>"
                << "<td>" << stat << "</td>"
                << "<td>" << t.progress << "%</td>"
                << "<td>" << t.elapsedSeconds << "</td>"
                << "<td>" << t.outputPath.toHtmlEscaped() << "</td></tr>";
        }
        out << "</table>";
    }

    out << "</body></html>";
    file.close();
    qDebug() << "[JsonReportRepository] HTML exported:" << filePath;
    return true;
}

bool JsonReportRepository::exportText(const ProcessingReport& report, const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "[JsonReportRepository] cannot write text:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out << "=== " << report.reportTitle << " ===\n"
        << "Generated: " << report.generatedAt.toString(Qt::ISODate) << "\n"
        << "Total Tasks: " << report.totalTasks << "\n"
        << "Success: " << report.successCount << "  Failed: " << report.failureCount << "\n"
        << "Total Duration: " << report.totalDurationSeconds << "s\n\n";

    if (!report.steps.isEmpty())
    {
        out << "--- Processing Steps ---\n";
        for (const auto& s : report.steps)
            out << "  [" << s.status << "] " << s.stepName << "  "
                << s.durationSeconds << "s  " << s.outputPath
                << (s.errorMessage.isEmpty() ? "" : "  ERR: " + s.errorMessage) << "\n";
    }

    if (!report.taskSummaries.isEmpty())
    {
        out << "\n--- Task Summaries ---\n";
        static const char* names[] = {"Pending","Running","Paused","Completed","Failed","Cancelled"};
        for (const auto& t : report.taskSummaries)
            out << "  #" << t.taskId << " " << t.taskName << " [" << t.taskType << "] "
                << names[qBound(0, t.status, 5)] << "  " << t.progress << "%  "
                << t.elapsedSeconds << "s\n";
    }

    file.close();
    qDebug() << "[JsonReportRepository] text exported:" << filePath;
    return true;
}
