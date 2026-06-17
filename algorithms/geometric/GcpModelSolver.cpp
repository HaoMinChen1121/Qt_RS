#include "GcpModelSolver.h"
#include <cmath>
#include <algorithm>

// ── 小型线性方程组求解：高斯消元（列主元）──
static bool solveLinearSystem(QVector<double>& A, QVector<double>& b, int n)
{
    // A: n×n 按行存储, b: n 向量，原地求解，结果存回 b
    for (int col = 0; col < n; ++col)
    {
        // 选列主元
        int maxRow = col;
        double maxVal = std::abs(A[col * n + col]);
        for (int row = col + 1; row < n; ++row)
        {
            double v = std::abs(A[row * n + col]);
            if (v > maxVal) { maxVal = v; maxRow = row; }
        }
        if (maxVal < 1e-15)
            return false;

        // 交换行
        if (maxRow != col)
        {
            for (int j = col; j < n; ++j)
                std::swap(A[col * n + j], A[maxRow * n + j]);
            std::swap(b[col], b[maxRow]);
        }

        // 消元
        double pivot = A[col * n + col];
        for (int row = col + 1; row < n; ++row)
        {
            double factor = A[row * n + col] / pivot;
            A[row * n + col] = 0;
            for (int j = col + 1; j < n; ++j)
                A[row * n + j] -= factor * A[col * n + j];
            b[row] -= factor * b[col];
        }
    }

    // 回代
    for (int i = n - 1; i >= 0; --i)
    {
        double sum = b[i];
        for (int j = i + 1; j < n; ++j)
            sum -= A[i * n + j] * b[j];
        b[i] = sum / A[i * n + i];
    }
    return true;
}

// ═══════════════════════════════════════════════════
//  多项式模型
// ═══════════════════════════════════════════════════

int GcpModelSolver::termCount(int order)
{
    return (order + 1) * (order + 2) / 2;
}

int GcpModelSolver::minGcpCountPolynomial(int order)
{
    return termCount(order);
}

double GcpModelSolver::polyTerm(int k, double col, double row, int order)
{
    // 按 (row^p * col^q) 展开, p+q <= order
    // k 的排序：(0,0), (0,1), (1,0), (0,2), (1,1), (2,0), ...
    int idx = 0;
    for (int d = 0; d <= order; ++d)
    {
        for (int p = 0; p <= d; ++p)
        {
            int q = d - p;
            if (idx == k)
                return std::pow(row, p) * std::pow(col, q);
            ++idx;
        }
    }
    return 0;
}

GcpModel GcpModelSolver::fitPolynomial(const QVector<Gcp>& gcps, int order)
{
    GcpModel model;
    model.type = QString("Polynomial%1").arg(order);
    model.minGcpCount = minGcpCountPolynomial(order);

    int nGcp = gcps.size();
    int nTerm = termCount(order);

    if (nGcp < model.minGcpCount || nTerm <= 0)
    {
        model.overallRmse = -1;
        return model;
    }

    // 分别拟合 X 和 Y：A^T A x = A^T b
    auto solveCoef = [&](const QVector<double>& targets) -> QVector<double>
    {
        QVector<double> ATA(nTerm * nTerm, 0);
        QVector<double> ATb(nTerm, 0);

        for (int i = 0; i < nGcp; ++i)
        {
            double col = gcps[i].srcX;
            double row = gcps[i].srcY;
            double t = targets[i];

            for (int p = 0; p < nTerm; ++p)
            {
                double ap = polyTerm(p, col, row, order);
                ATb[p] += ap * t;
                for (int q = 0; q < nTerm; ++q)
                    ATA[p * nTerm + q] += ap * polyTerm(q, col, row, order);
            }
        }

        if (!solveLinearSystem(ATA, ATb, nTerm))
            return {};

        return ATb;
    };

    // 参考 X 方向
    QVector<double> refXs(nGcp);
    for (int i = 0; i < nGcp; ++i) refXs[i] = gcps[i].refX;
    QVector<double> coeffsX = solveCoef(refXs);

    // 参考 Y 方向
    QVector<double> refYs(nGcp);
    for (int i = 0; i < nGcp; ++i) refYs[i] = gcps[i].refY;
    QVector<double> coeffsY = solveCoef(refYs);

    if (coeffsX.isEmpty() || coeffsY.isEmpty())
    {
        model.overallRmse = -1;
        return model;
    }

    // 合并系数：[coeffsX..., coeffsY...]，共 2*nTerm 个
    model.coefficients.resize(2 * nTerm);
    for (int i = 0; i < nTerm; ++i)
    {
        model.coefficients[i]            = coeffsX[i];
        model.coefficients[nTerm + i]    = coeffsY[i];
    }

    return model;
}

void GcpModelSolver::evalPolynomial(const GcpModel& model, int order,
                                    double col, double row, double& outX, double& outY)
{
    int nTerm = termCount(order);
    outX = outY = 0;

    for (int k = 0; k < nTerm; ++k)
    {
        double term = polyTerm(k, col, row, order);
        outX += model.coefficients[k] * term;
        outY += model.coefficients[nTerm + k] * term;
    }
}

// ═══════════════════════════════════════════════════
//  TPS 模型
// ═══════════════════════════════════════════════════

int GcpModelSolver::minGcpCountTPS()
{
    return 3; // 至少 3 个点用于仿射部分
}

double GcpModelSolver::tpsKernelR2(double r2)
{
    if (r2 < 1e-30) return 0;
    return r2 * std::log(std::sqrt(r2));
}

double GcpModelSolver::tpsKernel(double r)
{
    return tpsKernelR2(r * r);
}

GcpModel GcpModelSolver::fitTPS(const QVector<Gcp>& gcps, double smoothing)
{
    GcpModel model;
    model.type = "TPS";
    model.minGcpCount = minGcpCountTPS();

    int n = gcps.size();
    if (n < model.minGcpCount)
    {
        model.overallRmse = -1;
        return model;
    }

    // 系统大小: n (翘曲系数) + 3 (仿射系数 a0 + a1*x + a2*y)
    int m = n + 3;

    auto fitComponent = [&](const QVector<double>& targets) -> QVector<double>
    {
        QVector<double> A(m * m, 0);
        QVector<double> b(m, 0);

        // K 矩阵: K_ij = φ(||p_i - p_j||)
        for (int i = 0; i < n; ++i)
        {
            double xi = gcps[i].srcX, yi = gcps[i].srcY;
            for (int j = i; j < n; ++j)
            {
                double dx = xi - gcps[j].srcX;
                double dy = yi - gcps[j].srcY;
                double val = tpsKernelR2(dx * dx + dy * dy);
                // 对角线上加平滑项
                if (i == j) val += smoothing;
                A[i * m + j] = A[j * m + i] = val;
            }
        }

        // P 矩阵: [1, x_i, y_i]  (行 i, 列 n..n+2)
        for (int i = 0; i < n; ++i)
        {
            A[i * m + n + 0] = A[(n + 0) * m + i] = 1;
            A[i * m + n + 1] = A[(n + 1) * m + i] = gcps[i].srcX;
            A[i * m + n + 2] = A[(n + 2) * m + i] = gcps[i].srcY;
        }
        // 右下 3×3 = 0

        // 右侧: target values 填前 n 项，后面 3 项 = 0
        for (int i = 0; i < n; ++i)
            b[i] = targets[i];

        if (!solveLinearSystem(A, b, m))
            return {};

        return b; // 解: [w_1..w_n, a0, a1, a2]
    };

    QVector<double> refXs(n), refYs(n);
    for (int i = 0; i < n; ++i)
    {
        refXs[i] = gcps[i].refX;
        refYs[i] = gcps[i].refY;
    }

    QVector<double> solX = fitComponent(refXs);
    QVector<double> solY = fitComponent(refYs);

    if (solX.isEmpty() || solY.isEmpty())
    {
        model.overallRmse = -1;
        return model;
    }

    // 合并：[w_1..w_n, a0x, a1x, a2x, w_1..w_n, a0y, a1y, a2y]
    model.coefficients.resize(2 * (n + 3));
    for (int i = 0; i < n + 3; ++i)
    {
        model.coefficients[i]              = solX[i];
        model.coefficients[n + 3 + i]      = solY[i];
    }

    return model;
}

void GcpModelSolver::evalTPS(const GcpModel& model, const QVector<Gcp>& gcps,
                             double col, double row, double& outX, double& outY)
{
    int n = gcps.size();
    int m = n + 3;

    auto evalComponent = [&](int offset) -> double
    {
        double val = 0;
        // 仿射部分
        val += model.coefficients[offset + n + 0];             // a0
        val += model.coefficients[offset + n + 1] * col;       // a1 * x
        val += model.coefficients[offset + n + 2] * row;       // a2 * y
        // 翘曲部分
        for (int i = 0; i < n; ++i)
        {
            double dx = col - gcps[i].srcX;
            double dy = row - gcps[i].srcY;
            double r2 = dx * dx + dy * dy;
            val += model.coefficients[offset + i] * tpsKernelR2(r2);
        }
        return val;
    };

    outX = evalComponent(0);       // X 分量从 coefficients[0] 开始
    outY = evalComponent(m);       // Y 分量从 coefficients[m] 开始
}

void GcpModelSolver::evalModel(const GcpModel& model, const QVector<Gcp>& gcpsForTPS,
                               double col, double row, double& outX, double& outY)
{
    if (model.type == "TPS")
        evalTPS(model, gcpsForTPS, col, row, outX, outY);
    else
    {
        int order = model.type.mid(10).toInt();
        evalPolynomial(model, order, col, row, outX, outY);
    }
}

// ═══════════════════════════════════════════════════
//  RMSE 计算
// ═══════════════════════════════════════════════════

double GcpModelSolver::computeRMSE(QVector<Gcp>& gcps, const GcpModel& model)
{
    if (gcps.isEmpty()) return 0;

    bool isTPS = model.type == "TPS";
    int order = 0;
    if (model.type.startsWith("Polynomial"))
        order = model.type.mid(10).toInt();

    double sumSq = 0;
    for (auto& gcp : gcps)
    {
        double px, py;
        if (isTPS)
            evalTPS(model, gcps, gcp.srcX, gcp.srcY, px, py);
        else
            evalPolynomial(model, order, gcp.srcX, gcp.srcY, px, py);

        double dx = px - gcp.refX;
        double dy = py - gcp.refY;
        gcp.residual = std::sqrt(dx * dx + dy * dy);
        sumSq += gcp.residual * gcp.residual;
    }

    return std::sqrt(sumSq / gcps.size());
}
