#ifndef WAVELETFUSION_H
#define WAVELETFUSION_H

#include "IFusionAlgorithm.h"

class WaveletFusion : public IFusionAlgorithm
{
public:
    AlgorithmResult fuse(const QString& panPath,
                         const QString& msPath,
                         const QString& outputPath,
                         const ImageFusionParams& params,
                         ProgressCallback progress) override;
};

#endif // WAVELETFUSION_H
