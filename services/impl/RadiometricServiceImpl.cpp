#include "RadiometricServiceImpl.h"
#include "controllers/workers/RadiometricWorker.h"
#include "controllers/WorkerManager.h"
#include "dataaccess/IRasterReader.h"
#include "dataaccess/IRasterWriter.h"
#include "dataaccess/ISensorProduct.h"
#include "dataaccess/SensorProductFactory.h"
#include <QThread>
#include <QDebug>

RadiometricServiceImpl::RadiometricServiceImpl(IRasterReader* reader,
                                                 IRasterWriter* writer,
                                                 WorkerManager* workerManager,
                                                 QObject* parent)
    : IRadiometricService()
    , mReader(reader)
    , mWriter(writer)
    , mWorkerManager(workerManager)
    {
    setParent(parent);
}

// ─── 执行 ────────────────────────────────────────────────────────────────────

void RadiometricServiceImpl::execute(const RadiometricCorrectionParams& params)
{
    qDebug() << "[RadiometricService] execute: sensor=" << params.sensorType
             << "calibration=" << params.calibrationType << "atm=" << params.atmModel
             << "files=" << params.inputFiles.size();

    if (mCurrentWorker)
    {
        mCurrentWorker->requestCancel();
        cleanupWorker();
    }

    // 从产品提取 SensorInfo
    SensorInfo info;
    info.sensorType = params.sensorType;

    QString metaPath = params.metadataFile;
    if (metaPath.isEmpty() && !params.inputFiles.isEmpty())
        metaPath = params.inputFiles.first();

    auto tryOpenProduct = [](const QString& path) -> SensorInfo
    {
        QScopedPointer<ISensorProduct> p(createSensorProduct(path));
        if (p && p->open(path))
            return p->sensorInfo();
        return {};
    };

    if (!metaPath.isEmpty())
    {
        info = tryOpenProduct(metaPath);

        // 直接匹配失败时，从波段文件路径向上查找 .SAFE / .zip
        if (info.bands.isEmpty())
        {
            QFileInfo fi(metaPath);
            QDir dir = fi.isDir() ? QDir(metaPath) : fi.absoluteDir();

            // 向上遍历目录树，查找 .SAFE 根或 .zip 文件
            bool found = false;
            for (int level = 0; level < 5 && !found; ++level)
            {
                QString absPath = dir.absolutePath();

                // 检查当前目录是否是 .SAFE 目录
                if (absPath.endsWith(".SAFE", Qt::CaseInsensitive))
                {
                    info = tryOpenProduct(absPath);
                    if (!info.bands.isEmpty()) found = true;
                }

                // 检查当前目录是否包含 manifest.safe
                if (!found && QFileInfo::exists(absPath + "/manifest.safe"))
                {
                    info = tryOpenProduct(absPath);
                    if (!info.bands.isEmpty()) found = true;
                }

                // 检查当前目录中是否有 .SAFE 子目录
                if (!found)
                {
                    QStringList safeDirs = dir.entryList(
                        {"*.SAFE"}, QDir::Dirs | QDir::NoDotAndDotDot);
                    for (const QString& sd : safeDirs)
                    {
                        info = tryOpenProduct(absPath + "/" + sd);
                        if (!info.bands.isEmpty()) { found = true; break; }
                    }
                }

                // 检查 /vsizip/ 格式：从完整路径中提取 ZIP 路径
                if (!found && metaPath.startsWith("/vsizip/"))
                {
                    QString vsiPath = metaPath.mid(QStringLiteral("/vsizip/").length());
                    int safeIdx = vsiPath.indexOf(".SAFE", Qt::CaseInsensitive);
                    if (safeIdx >= 0)
                    {
                        QString zipPath = vsiPath.left(safeIdx) + ".zip";
                        info = tryOpenProduct(zipPath);
                        if (!info.bands.isEmpty()) found = true;
                    }
                }

                if (dir.isRoot()) break;
                dir.cdUp();
            }
        }
    }

    info.sensorType = params.sensorType; // 优先使用用户指定的类型

    auto* worker = new RadiometricWorker(mReader, mWriter, params, info);
    mCurrentWorker = worker;

    QThread* thread = mWorkerManager->allocateThread(QStringLiteral("Radiometric"));
    mCurrentThread = thread;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &RadiometricWorker::process);
    connect(worker, &TaskWorker::progressChanged, this, &IRadiometricService::progressChanged);
    connect(worker, &TaskWorker::finished, this, [this](bool success, const QString& path)
    {
        mRunning = false;
        emit finished(success, path);
        cleanupWorker();
    });
    connect(worker, &TaskWorker::errorOccurred, this, &IRadiometricService::errorOccurred);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);

    mRunning = true;
    thread->start();
}

void RadiometricServiceImpl::executeBatch(const QList<RadiometricCorrectionParams>& batch)
{
    qDebug() << "[RadiometricService] executeBatch: count=" << batch.size();
    if (batch.isEmpty()) return;
    // 批量模式：串联执行，每个完成后通过 finished 信号知会 UI，再处理下一个
    // 简化实现：依次执行（信号异步，此处直接遍历）
    for (const auto& p : batch)
        execute(p);
}

void RadiometricServiceImpl::cancel()
{
    qDebug() << "[RadiometricService] cancel";
    if (mCurrentWorker)
        mCurrentWorker->requestCancel();
    mRunning = false;
}

bool RadiometricServiceImpl::isRunning() const
{
    return mRunning;
}

void RadiometricServiceImpl::cleanupWorker()
{
    if (mCurrentThread)
    {
        mCurrentThread->quit();
        mCurrentThread->wait(5000);
        mCurrentThread = nullptr;
    }
    mCurrentWorker = nullptr;
}
