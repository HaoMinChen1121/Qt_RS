#ifndef PCAFUSION_H
#define PCAFUSION_H

#include "IFusionAlgorithm.h"

class PcaFusion : public IFusionAlgorithm
{
public:
    AlgorithmResult fuse(const QString& panPath,
                         const QString& msPath,
                         const QString& outputPath,
                         const ImageFusionParams& params,
                         ProgressCallback progress) override;
};

#endif // PCAFUSION_H
