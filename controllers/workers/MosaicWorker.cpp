#include "MosaicWorker.h"
#include "algorithms/mosaic/MosaicAssembler.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>

MosaicWorker::MosaicWorker(const MosaicParams& params, QObject* parent)
    : TaskWorker(parent)
    , mParams(params)
    {
}

void MosaicWorker::process()
{
    qDebug() << "[MosaicWorker] process: images=" << mParams.inputImages.size()
             << "balance=" << mParams.colorBalanceMethod
             << "seamline=" << mParams.seamlineMethod;

    if (mParams.inputImages.isEmpty())
    {
        emit errorOccurred(QStringLiteral("No input images for mosaic"));
        emit finished(false, QString());
        return;
    }

    QString outputPath = mParams.outputPath;
    if (outputPath.isEmpty())
    {
        QFileInfo fi(mParams.inputImages.first());
        outputPath = fi.absolutePath() + QStringLiteral("/mosaic.tif");
    }
    QDir().mkpath(QFileInfo(outputPath).absolutePath());

    MosaicParams runParams = mParams;
    runParams.outputPath = outputPath;

    ProgressCallback progressFn = [this](int pct, const QString& msg) -> bool
    {
        if (isCancelled())
            return false;
        emit progressChanged(pct, msg);
        return true;
    };

    MosaicAssembler assembler;
    AlgorithmResult result = assembler.assemble(runParams, progressFn);

    if (!result.success)
    {
        emit errorOccurred(result.errorMessage);
        emit finished(false, QString());
        return;
    }

    emit finished(true, outputPath);
}
