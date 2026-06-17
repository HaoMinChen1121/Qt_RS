#ifndef MOSAICASSEMBLER_H
#define MOSAICASSEMBLER_H

#include "domain/params/MosaicParams.h"
#include "algorithms/common/AlgorithmResult.h"
#include "algorithms/common/ProgressCallback.h"
#include <QString>

class MosaicAssembler
{
public:
    AlgorithmResult assemble(const MosaicParams& params,
                              ProgressCallback progress);
};

#endif // MOSAICASSEMBLER_H
