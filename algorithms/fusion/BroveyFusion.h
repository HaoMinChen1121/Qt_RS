#ifndef BROVEYFUSION_H
#define BROVEYFUSION_H

#include "IFusionAlgorithm.h"

class BroveyFusion : public IFusionAlgorithm
{
public:
    AlgorithmResult fuse(const QString& panPath,
                         const QString& msPath,
                         const QString& outputPath,
                         const ImageFusionParams& params,
                         ProgressCallback progress) override;
};

#endif // BROVEYFUSION_H
