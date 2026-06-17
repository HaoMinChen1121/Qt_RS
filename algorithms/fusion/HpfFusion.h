#ifndef HPFFUSION_H
#define HPFFUSION_H

#include "IFusionAlgorithm.h"

class HpfFusion : public IFusionAlgorithm
{
public:
    AlgorithmResult fuse(const QString& panPath,
                         const QString& msPath,
                         const QString& outputPath,
                         const ImageFusionParams& params,
                         ProgressCallback progress) override;
};

#endif // HPFFUSION_H
