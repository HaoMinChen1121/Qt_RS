#ifndef GRAMSCHMIDTFUSION_H
#define GRAMSCHMIDTFUSION_H

#include "IFusionAlgorithm.h"

class GramSchmidtFusion : public IFusionAlgorithm
{
public:
    AlgorithmResult fuse(const QString& panPath,
                         const QString& msPath,
                         const QString& outputPath,
                         const ImageFusionParams& params,
                         ProgressCallback progress) override;
};

#endif // GRAMSCHMIDTFUSION_H
