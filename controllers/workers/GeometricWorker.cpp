#include "GeometricWorker.h"
#include "algorithms/geometric/GeometricCorrector.h"
#include "algorithms/common/ProgressCallback.h"
#include <QDebug>

GeometricWorker::GeometricWorker(const GeometricCorrectionParams& params,
                                 QObject* parent)
    : TaskWorker(parent)
    , mParams(params)
{
}

void GeometricWorker::process()
{
    if (mParams.sourceImage.isEmpty())
    {
        emit errorOccurred("No source image specified");
        emit finished(false, QString());
        return;
    }

    // 构造进度回调，包装取消检查
    ProgressCallback progressFn = [this](int percent, const QString& msg) -> bool
    {
        if (isCancelled())
        {
            emit errorOccurred("Cancelled");
            return false;
        }
        emit progressChanged(percent, msg);
        return true;
    };

    GeometricCorrector corrector;
    GeometricResult result = corrector.correct(mParams, progressFn);

    if (!result.success)
    {
        emit errorOccurred(result.errorMessage);
        emit finished(false, QString());
        return;
    }

    // 发出详细结果信号（含误差统计）
    QString detailMsg = QString("GCPs: %1/%2, RMSE: %3 px, GPU: %4, "
                                "Match: %5 s, Correct: %6 s")
        .arg(result.inlierGcps)
        .arg(result.totalGcps)
        .arg(result.model.overallRmse, 0, 'f', 3)
        .arg(result.gpuUsed ? "Yes" : "No")
        .arg(result.matchTimeSec, 0, 'f', 1)
        .arg(result.correctTimeSec, 0, 'f', 1);

    qDebug() << "[GeometricWorker]" << detailMsg;

    emit progressChanged(100, detailMsg);
    emit finished(true, result.outputPath);
}
