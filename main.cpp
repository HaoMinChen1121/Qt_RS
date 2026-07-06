#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include "SARibbonBar.h"
#include "controllers/ApplicationController.h"
#include <gdal_priv.h>
// 重定向qdebug的打印
void log_out_put(QtMsgType type, const QMessageLogContext& context, const QString& msg);

/**
 * @brief 重定向qdebug的打印
 * @param type
 * @param context
 * @param msg
 */
void log_out_put(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    QByteArray localMsg = msg.toLocal8Bit();

    switch (type)
    {
    case QtDebugMsg:
        fprintf(stdout, "%s |[Debug] (%s[%u],%s)\n", localMsg.constData(), context.function, context.line, context.file);
        break;

    case QtWarningMsg:
        fprintf(stdout, "%s |[Warning] (%s[%u],%s)\n", localMsg.constData(), context.function, context.line, context.file);
        break;

    case QtCriticalMsg:
        fprintf(stdout, "%s |[Critical] (%s[%u],%s)\n", localMsg.constData(), context.function, context.line, context.file);
        break;

    case QtFatalMsg:
        fprintf(stdout, "%s |[Fatal] (%s[%u],%s)\n", localMsg.constData(), context.function, context.line, context.file);
        abort();
        break;

    default:
        fprintf(stdout, "%s |[Debug](%s[%u],%s)\n", localMsg.constData(), context.function, context.line, context.file);
        break;
    }
#ifndef QT_NO_DEBUG_OUTPUT
    fflush(stdout);
#endif
}


/**
 * @file main.cpp
 * @brief 应用程序入口：初始化高 DPI、日志重定向、资源加载、字体设置并创建主窗口
 *
 * 说明：
 * - 调用 `SARibbonBar::initHighDpi()` 以处理高分辨率屏幕的缩放问题（在需要高 DPI 支持时调用）。
 * - 创建 `QApplication` 实例并通过 `qInstallMessageHandler(log_out_put)` 安装自定义的 Qt 日志处理器，
 *   将 `qDebug()`/`qWarning()`/`qCritical()`/`qFatal()` 的输出重定向到 stdout 并包含函数/行号/文件信息。
 * - 在以静态方式链接 SARibbon 库时，使用 `Q_INIT_RESOURCE(SARibbonResource)` 显式加载资源（受宏 `SA_RIBBON_BAR_NO_EXPORT` 控制）。
 * - 设置应用全局字体为中文常用字体“微软雅黑”以保证界面文本显示一致。
 * - 使用 `QElapsedTimer` 统计主窗口构建耗时，并通过 `qDebug()` 输出便于性能调试。
 * - 显示主窗口并进入 Qt 事件循环，函数返回 `QApplication::exec()` 的退出码。
 *
 * 注意事项：
 * - `log_out_put` 是全局的消息处理函数，如果应用存在多线程，请确保存取共享资源的线程安全性。
 * - 发布构建时可根据需要调整或移除详细日志输出以避免泄露实现细节或影响性能。
 *
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return int 应用退出码（来自 `QApplication::exec()`）
 */
int main(int argc, char* argv[])
{
    // QGIS / GDAL / PROJ 运行时数据路径 — 必须在 DLL 加载前设置
    // 路径基于项目编译时的 QGIS 安装位置 (../qgis-ltr/ = F:/GIS_RJKF/qgis-ltr/)
    qputenv("PROJ_LIB", "E:/GIS_QT/share/proj");
    qputenv("GDAL_DATA", "E:/GIS_QT/share/gdal");

    // 以下是针对高分屏的设置，有高分屏需求都需要按照下面进行设置
    SARibbonBar::initHighDpi();

    QApplication a(argc, argv);
    qInstallMessageHandler(log_out_put);
#ifdef SA_RIBBON_BAR_NO_EXPORT
    Q_INIT_RESOURCE(SARibbonResource);  // 针对静态库的资源加载
#endif
    QFont f = a.font();
    f.setFamily(u8"微软雅黑");
    a.setFont(f);
    QElapsedTimer cost;

    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "YES");
    GDALAllRegister();

    // Suppress known harmless GDAL warnings from OpenJPEG driver parsing
    // Sentinel-2 JP2 files: "missing [", "Empty filename passed to function"
    CPLSetErrorHandler([](CPLErr eClass, int code, const char* msg)
    {
        if (msg && (strstr(msg, "missing [") || strstr(msg, "Empty filename")))
            return;  // silently ignore
        CPLDefaultErrorHandler(eClass, code, msg);
    });

    cost.start();
    MainWindow w;
    qDebug() << "window build cost:" << cost.elapsed() << " ms";

    // 创建应用控制器（组合根），建立 UI ↔ 业务逻辑层的信号连接
    ApplicationController appController(&w);
    appController.initialize();

    w.show();

    return (a.exec());
}
