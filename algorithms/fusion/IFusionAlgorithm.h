#ifndef IFUSIONALGORITHM_H
#define IFUSIONALGORITHM_H

#include "algorithms/common/AlgorithmResult.h"
#include "algorithms/common/ProgressCallback.h"
#include "domain/params/ImageFusionParams.h"
#include <QString>

class IFusionAlgorithm
{
public:
    virtual ~IFusionAlgorithm() = default;

    virtual AlgorithmResult fuse(const QString& panPath,
                                 const QString& msPath,
                                 const QString& outputPath,
                                 const ImageFusionParams& params,
                                 ProgressCallback progress) = 0;
};

#endif // IFUSIONALGORITHM_H
