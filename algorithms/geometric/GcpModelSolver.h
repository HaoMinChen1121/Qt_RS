#ifndef GCPMODELSOLVER_H
#define GCPMODELSOLVER_H

#include "domain/params/GeometricCorrectionParams.h"

class GcpModelSolver
{
public:
    // Polynomial least-squares fit, order = 1~5
    static GcpModel fitPolynomial(const QVector<Gcp>& gcps, int order);

    // Thin Plate Spline fit, smoothing >= 0 (0 = exact interpolation)
    static GcpModel fitTPS(const QVector<Gcp>& gcps, double smoothing = 0.0);

    // Compute RMSE and fill each GCP's residual
    static double computeRMSE(QVector<Gcp>& gcps, const GcpModel& model);

    // Minimum GCP count for polynomial model
    static int minGcpCountPolynomial(int order);

    // Minimum GCP count for TPS model
    static int minGcpCountTPS();

    // Evaluate polynomial at (col, row), returns transformed (x, y)
    static void evalPolynomial(const GcpModel& model, int order,
                               double col, double row, double& outX, double& outY);

    // Evaluate TPS at (col, row)
    static void evalTPS(const GcpModel& model, const QVector<Gcp>& gcps,
                        double col, double row, double& outX, double& outY);

    // Evaluate any model (polynomial or TPS) at (col, row).
    // For TPS: gcpsForTPS.srcX/srcY serve as kernel centers.
    // To evaluate a correction model (ref→src), pass swapped GCPs where
    // .srcX = reference X, .srcY = reference Y.
    static void evalModel(const GcpModel& model, const QVector<Gcp>& gcpsForTPS,
                          double col, double row, double& outX, double& outY);

private:
    // Polynomial term count: (order+1)*(order+2)/2
    static int termCount(int order);

    // Compute single polynomial term k at (col, row)
    static double polyTerm(int k, double col, double row, int order);

    // TPS kernel functions
    static double tpsKernel(double r);
    static double tpsKernelR2(double r2);
};

#endif // GCPMODELSOLVER_H
