#ifndef IHSFUSION_H
#define IHSFUSION_H

#include "IFusionAlgorithm.h"

class IhsFusion : public IFusionAlgorithm
{
public:
    AlgorithmResult fuse(const QString& panPath,
                         const QString& msPath,
                         const QString& outputPath,
                         const ImageFusionParams& params,
                         ProgressCallback progress) override;
};

#endif // IHSFUSION_H
