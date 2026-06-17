#include "FusionWorker.h"
#include "dataaccess/IRasterReader.h"
#include "dataaccess/IRasterWriter.h"
#include "algorithms/fusion/IFusionAlgorithm.h"
#include "algorithms/fusion/IhsFusion.h"
#include "algorithms/fusion/BroveyFusion.h"
#include "algorithms/fusion/GramSchmidtFusion.h"
#include "algorithms/fusion/PcaFusion.h"
#include "algorithms/fusion/HpfFusion.h"
#include "algorithms/fusion/WaveletFusion.h"
#include "algorithms/fusion/QualityMetricsEvaluator.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>

FusionWorker::FusionWorker(IRasterReader* reader,
                             IRasterWriter* writer,
                             const ImageFusionParams& params,
                             QObject* parent)
    : TaskWorker(parent)
    , mReader(reader)
    , mWriter(writer)
    , mParams(params)
    {
}

void FusionWorker::process()
{
    qDebug() << "[FusionWorker] process: algo=" << mParams.algorithm
             << "pan=" << mParams.panchromaticImage
             << "ms=" << mParams.multispectralImage;

    if (mParams.panchromaticImage.isEmpty() || mParams.multispectralImage.isEmpty())
    {
        emit errorOccurred(QStringLiteral("Panchromatic or multispectral image not specified"));
        emit finished(false, QString());
        return;
    }

    QString outputPath = mParams.outputPath;
    if (outputPath.isEmpty())
    {
        QFileInfo fi(mParams.multispectralImage);
        outputPath = fi.absolutePath() + QStringLiteral("/fused_")
                     + mParams.algorithm.toLower() + QStringLiteral(".tif");
    }
    QDir().mkpath(QFileInfo(outputPath).absolutePath());

    ProgressCallback progressFn = [this](int pct, const QString& msg) -> bool {
        if (isCancelled()) { emit errorOccurred(QStringLiteral("Cancelled")); return false; }
        emit progressChanged(pct, msg);
        return true;
    };

    // 选择算法
    IFusionAlgorithm* algo = nullptr;
    QString algoName = mParams.algorithm.toLower();
    if (algoName == QStringLiteral("ihs"))             algo = new IhsFusion();
    else if (algoName == QStringLiteral("brovey"))     algo = new BroveyFusion();
    else if (algoName.contains(QStringLiteral("gram"))) algo = new GramSchmidtFusion();
    else if (algoName == QStringLiteral("pca"))        algo = new PcaFusion();
    else if (algoName == QStringLiteral("hpf"))        algo = new HpfFusion();
    else if (algoName == QStringLiteral("wavelet"))    algo = new WaveletFusion();
    else { algo = new IhsFusion(); qDebug() << "[FusionWorker] unknown algo, defaulting to IHS"; }

    AlgorithmResult result = algo->fuse(mParams.panchromaticImage,
                                         mParams.multispectralImage,
                                         outputPath, mParams, progressFn);
    delete algo;

    if (!result.success)
    {
        emit errorOccurred(result.errorMessage);
        emit finished(false, QString());
        return;
    }

    emit progressChanged(90, QStringLiteral("Computing quality metrics..."));

    // 质量评价
    bool needMetrics = mParams.computeCorrelationCoefficient
                    || mParams.computeAverageGradient
                    || mParams.computeRMSE
                    || mParams.computeSSIM;
    if (needMetrics)
    {
        FusionQualityMetrics metrics = QualityMetricsEvaluator::evaluate(
            outputPath, mParams.multispectralImage, mParams);
        emit qualityMetricsReady(metrics);
    }

    emit finished(true, outputPath);
}
