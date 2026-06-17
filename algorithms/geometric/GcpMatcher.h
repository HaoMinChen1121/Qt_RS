#ifndef GCPMATCHER_H
#define GCPMATCHER_H

#include "domain/params/GeometricCorrectionParams.h"

class GcpMatcher
{
public:
    // Auto-match using method (SIFT/SURF/NCC)
    static QVector<Gcp> autoMatch(const QString& srcImage,
                                  const QString& refImage,
                                  const GcpMatchingParams& params,
                                  int* outTotalMatches = nullptr,
                                  int* outInliers = nullptr);

    // SIFT feature matching (uses overview/pyramid to reduce memory)
    static QVector<Gcp> matchSIFT(const QString& srcImage,
                                  const QString& refImage,
                                  const GcpMatchingParams& params);

    // SURF feature matching (uses overview/pyramid to reduce memory)
    static QVector<Gcp> matchSURF(const QString& srcImage,
                                  const QString& refImage,
                                  const GcpMatchingParams& params);

    // NCC template matching at overview level
    static QVector<Gcp> matchNCC(const QString& srcImage,
                                 const QString& refImage,
                                 const GcpMatchingParams& params);

    // Semi-auto refinement: uses windowed full-resolution reads per GCP
    static QVector<Gcp> refineByNCC(const QString& srcImage,
                                    const QString& refImage,
                                    const QVector<Gcp>& roughGcps,
                                    const GcpMatchingParams& params);
};

#endif // GCPMATCHER_H
