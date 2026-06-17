#include "SpectralProfileDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QCloseEvent>
#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <QPen>
#include <QBrush>
#include <cmath>

static const QColor sPalette[] = {
    QColor(0xE6,0x39,0x46), QColor(0x3B,0x7D,0xD8), QColor(0x2C,0xA0,0x2C),
    QColor(0xFF,0x7F,0x0E), QColor(0x94,0x62,0xBD), QColor(0x8C,0x56,0x4B),
    QColor(0xE3,0x77,0xC2), QColor(0x7F,0x7F,0x7F), QColor(0xBC,0xBD,0x22),
    QColor(0x17,0xBE,0xCF),
};
static const int sPaletteSize = 10;

// ============================================================================
// SpectralPlotWidget — custom QPainter spectral curve plot
// ============================================================================

class SpectralPlotWidget : public QWidget
{
public:
    using CursorCallback = std::function<void(const QString&)>;
    CursorCallback onCursorChanged;

    explicit SpectralPlotWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMouseTracking(true);
        setMinimumSize(300, 200);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void addCurve(const SpectralData& data, const QColor& color)
    {
        mCurves.append({data, color});
        mZoomStack.clear();
        update();
    }

    void clearAll()
    {
        mCurves.clear();
        mZoomStack.clear();
        mCursorPos = QPointF(-1, -1);
        update();
    }

    const auto& curves() const { return mCurves; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), Qt::white);

        if (mCurves.isEmpty())
        {
            p.setPen(Qt::gray);
            p.drawText(rect(), Qt::AlignCenter, "Click map to extract spectrum");
            return;
        }

        double xMin = mViewXMin, xMax = mViewXMax;
        double yMin = mViewYMin, yMax = mViewYMax;

        if (mZoomStack.isEmpty())
        {
            computeAutoRange(xMin, xMax, yMin, yMax);
            mViewXMin = xMin; mViewXMax = xMax;
            mViewYMin = yMin; mViewYMax = yMax;
        }

        if (xMax <= xMin) xMax = xMin + 1;
        if (yMax <= yMin) yMax = yMin + 0.01;

        const int mL = 60, mR = 20, mT = 15, mB = 35;
        int plotW = width() - mL - mR;
        int plotH = height() - mT - mB;
        if (plotW <= 0 || plotH <= 0) return;

        auto toX = [&](double v) { return mL + (v - xMin) / (xMax - xMin) * plotW; };
        auto toY = [&](double v) { return mT + (1.0 - (v - yMin) / (yMax - yMin)) * plotH; };

        // Grid
        p.setPen(QPen(QColor(0xE0,0xE0,0xE0), 0.5, Qt::DotLine));
        int nGridY = 5, nGridX = std::min(10, (int)(xMax - xMin + 1));
        if (nGridX < 2) nGridX = 5;
        for (int i = 0; i <= nGridY; ++i) {
            double yv = yMin + (yMax - yMin) * i / nGridY;
            p.drawLine(mL, (int)toY(yv), mL + plotW, (int)toY(yv));
        }
        for (int i = 0; i <= nGridX; ++i) {
            double xv = xMin + (xMax - xMin) * i / nGridX;
            p.drawLine((int)toX(xv), mT, (int)toX(xv), mT + plotH);
        }

        // Axes
        p.setPen(QPen(Qt::black, 1));
        p.drawLine(mL, mT + plotH, mL + plotW, mT + plotH);
        p.drawLine(mL, mT, mL, mT + plotH);

        bool hasWL = !mCurves[0].first.wavelengths.isEmpty();

        // X labels
        p.setPen(Qt::black);
        for (int i = 0; i <= nGridX; ++i) {
            double xv = xMin + (xMax - xMin) * i / nGridX;
            int sx = (int)toX(xv);
            QString label = hasWL ? QString::number(xv,'f',0) : QString::number((int)xv);
            p.drawText(QRect(sx - 25, mT + plotH + 2, 50, 15), Qt::AlignHCenter | Qt::AlignTop, label);
        }
        p.drawText(QRect(mL, mT + plotH + 18, plotW, 15), Qt::AlignHCenter,
            hasWL ? "Wavelength (nm)" : "Band Number");

        // Y labels
        for (int i = 0; i <= nGridY; ++i) {
            double yv = yMin + (yMax - yMin) * i / nGridY;
            int sy = (int)toY(yv);
            p.drawText(QRect(2, sy - 8, mL - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                QString::number(yv, 'g', 4));
        }
        p.save();
        p.translate(12, mT + plotH / 2);
        p.rotate(-90);
        p.drawText(QRect(-plotH/2, -12, plotH, 16), Qt::AlignHCenter, "Value");
        p.restore();

        // Curves
        for (const auto& cd : mCurves)
        {
            const SpectralData& sd = cd.first;
            const QColor& color = cd.second;
            int n = sd.bandValues.size();
            if (n == 0) continue;

            QVector<QPointF> pts(n);
            for (int i = 0; i < n; ++i) {
                double xv = hasWL ? sd.wavelengths[i] : (i + 1.0);
                double yv = sd.bandValues[i];
                if (std::isnan(yv) || std::isinf(yv)) yv = 0;
                pts[i] = QPointF(toX(xv), toY(yv));
            }

            p.setPen(QPen(color, 1.5));
            for (int i = 0; i < n - 1; ++i) p.drawLine(pts[i], pts[i+1]);

            p.setBrush(color);
            p.setPen(QPen(color.darker(150), 0.5));
            for (int i = 0; i < n; ++i) p.drawEllipse(pts[i], 3, 3);
        }

        // Legend
        int legX = mL + 8, legY = mT + 8;
        p.setFont(QFont("Segoe UI", 8));
        for (const auto& cd : mCurves) {
            QRect lr(legX, legY, 12, 12);
            p.fillRect(lr, cd.second);
            p.setPen(Qt::black);
            p.drawRect(lr);
            p.drawText(legX + 16, legY + 1, 200, 12, Qt::AlignLeft, cd.first.layerName);
            legY += 16;
        }

        // Cursor line
        if (mCursorPos.x() >= mL && mCursorPos.x() <= mL + plotW) {
            p.setPen(QPen(Qt::red, 0.5, Qt::DashLine));
            p.drawLine((int)mCursorPos.x(), mT, (int)mCursorPos.x(), mT + plotH);
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        mCursorPos = e->pos();
        update();

        if (onCursorChanged && !mCurves.isEmpty())
        {
            const int mL = 60, mR = 20, mT = 15, mB = 35;
            int plotW = width() - mL - mR;
            if (plotW <= 0) return;

            double frac = (e->pos().x() - mL) / (double)plotW;
            double xv = mViewXMin + frac * (mViewXMax - mViewXMin);
            bool hasWL = !mCurves[0].first.wavelengths.isEmpty();

            QString tip;
            for (const auto& cd : mCurves) {
                const SpectralData& sd = cd.first;
                int n = sd.bandValues.size();
                int best = -1; double bestDist = 1e30;
                for (int i = 0; i < n; ++i) {
                    double bx = hasWL ? sd.wavelengths[i] : (i + 1.0);
                    double d = std::abs(bx - xv);
                    if (d < bestDist) { bestDist = d; best = i; }
                }
                if (best >= 0) {
                    double bx = hasWL ? sd.wavelengths[best] : (best + 1.0);
                    tip += QString("B%1(%2nm)=%3\n")
                        .arg(best + 1).arg((int)bx).arg(sd.bandValues[best], 0, 'f', 6);
                }
            }
            if (!tip.isEmpty()) { tip.chop(1); QToolTip::showText(e->globalPos(), tip, this); }
        }
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::RightButton) {
            mZoomStack.clear();
            mViewXMin = mViewXMax = mViewYMin = mViewYMax = 0;
            update();
        } else if (e->button() == Qt::LeftButton) {
            mRubberStart = e->pos();
            mRubberBand = true;
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (mRubberBand && e->button() == Qt::LeftButton) {
            mRubberBand = false;
            QPoint p1 = mRubberStart, p2 = e->pos();
            int dx = std::abs(p2.x() - p1.x()), dy = std::abs(p2.y() - p1.y());
            if (dx > 10 && dy > 10) {
                const int mL = 60, mR = 20, mT = 15, mB = 35;
                int plotW = width() - mL - mR, plotH = height() - mT - mB;
                if (plotW > 0 && plotH > 0) {
                    double xMin = mViewXMin, xMax = mViewXMax;
                    double yMin = mViewYMin, yMax = mViewYMax;
                    double fx1 = (std::min(p1.x(), p2.x()) - mL) / (double)plotW;
                    double fx2 = (std::max(p1.x(), p2.x()) - mL) / (double)plotW;
                    double fy1 = (std::max(p1.y(), p2.y()) - mT) / (double)plotH;
                    double fy2 = (std::min(p1.y(), p2.y()) - mT) / (double)plotH;
                    mZoomStack.append({mViewXMin, mViewXMax, mViewYMin, mViewYMax});
                    mViewXMin = xMin + fx1 * (xMax - xMin);
                    mViewXMax = xMin + fx2 * (xMax - xMin);
                    mViewYMin = yMin + (1.0 - fy1) * (yMax - yMin);
                    mViewYMax = yMin + (1.0 - fy2) * (yMax - yMin);
                    update();
                }
            }
        }
    }

    void wheelEvent(QWheelEvent* e) override
    {
        double factor = (e->angleDelta().y() > 0) ? 0.9 : 1.1;
        double cx = mViewXMin + (mViewXMax - mViewXMin) * 0.5;
        double cy = mViewYMin + (mViewYMax - mViewYMin) * 0.5;
        mViewXMin = cx + (mViewXMin - cx) * factor;
        mViewXMax = cx + (mViewXMax - cx) * factor;
        mViewYMin = cy + (mViewYMin - cy) * factor;
        mViewYMax = cy + (mViewYMax - cy) * factor;
        update();
    }

private:
    void computeAutoRange(double& xMin, double& xMax, double& yMin, double& yMax)
    {
        xMin = 1e30; xMax = -1e30; yMin = 1e30; yMax = -1e30;
        bool hasWL = false;
        for (const auto& cd : mCurves) {
            const SpectralData& sd = cd.first;
            int n = sd.bandValues.size();
            if (n == 0) continue;
            hasWL = !sd.wavelengths.isEmpty();
            for (int i = 0; i < n; ++i) {
                double xv = hasWL ? sd.wavelengths[i] : (i + 1.0);
                double yv = sd.bandValues[i];
                if (std::isnan(yv) || std::isinf(yv)) continue;
                if (xv < xMin) xMin = xv; if (xv > xMax) xMax = xv;
                if (yv < yMin) yMin = yv; if (yv > yMax) yMax = yv;
            }
        }
        double dx = (xMax - xMin) * 0.05, dy = (yMax - yMin) * 0.05;
        if (dx < 0.5) dx = 0.5;
        if (dy < 1e-6) dy = std::max(yMax * 0.05, 1e-6);
        xMin -= dx; xMax += dx; yMin -= dy; yMax += dy;
    }

    struct CurveEntry { SpectralData first; QColor second; };
    QVector<CurveEntry> mCurves;
    QVector<QVector<double>> mZoomStack;
    double mViewXMin = 0, mViewXMax = 0, mViewYMin = 0, mViewYMax = 0;
    QPointF mCursorPos{-1, -1};
    QPoint mRubberStart;
    bool mRubberBand = false;
};

// ============================================================================
// SpectralProfileDialog
// ============================================================================

SpectralProfileDialog::SpectralProfileDialog(QWidget* parent)
    : QDialog(parent) { setupUI(); }

void SpectralProfileDialog::setupUI()
{
    setWindowTitle(tr("Spectral Profile"));
    setMinimumSize(600, 450);
    resize(750, 500);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    mPlot = new SpectralPlotWidget(this);
    mainLayout->addWidget(mPlot, 1);

    auto* bottom = new QHBoxLayout();
    auto* cursorLabel = new QLabel(tr("Move mouse over curves for values"), this);
    auto* pixelInfo   = new QLabel(this);
    auto* pinBtn      = new QPushButton(tr("Pin"), this);
    auto* clearBtn    = new QPushButton(tr("Clear"), this);
    auto* exportBtn   = new QPushButton(tr("Export CSV..."), this);
    pinBtn->setCheckable(true);
    pinBtn->setChecked(true);

    bottom->addWidget(cursorLabel);
    bottom->addStretch();
    bottom->addWidget(pixelInfo);
    bottom->addWidget(pinBtn);
    bottom->addWidget(clearBtn);
    bottom->addWidget(exportBtn);
    mainLayout->addLayout(bottom);

    connect(clearBtn, &QPushButton::clicked, this, [this, pixelInfo, cursorLabel]() {
        clearAll(); pixelInfo->clear();
        cursorLabel->setText(tr("Move mouse over curves for values"));
    });
    connect(exportBtn, &QPushButton::clicked, this, &SpectralProfileDialog::exportCsv);

    mPlot->onCursorChanged = [cursorLabel](const QString& tip) {
        QString t = tip;
        cursorLabel->setText(t.replace('\n', " | "));
    };

    // Store widgets for addProfile access
    mPinBtn = pinBtn;
    setProperty("pixelInfo", QVariant::fromValue((QWidget*)pixelInfo));
}

void SpectralProfileDialog::addProfile(const SpectralData& data)
{
    if (!mPinBtn->isChecked()) clearAll();

    QColor color = data.color.isValid() ? data.color
        : sPalette[mPinned.size() % sPaletteSize];

    SpectralData sd = data; sd.color = color;
    mPinned.append(sd);
    mPlot->addCurve(sd, color);

    auto* pi = qobject_cast<QLabel*>(property("pixelInfo").value<QWidget*>());
    if (pi) pi->setText(QString("%1 | Pixel(%2,%3) | Geo(%.0f,%.0f)")
        .arg(sd.layerName).arg((int)sd.pixelCol).arg((int)sd.pixelRow)
        .arg(sd.geoX, 0, 'f', 0).arg(sd.geoY, 0, 'f', 0));
}

void SpectralProfileDialog::clearAll() { mPinned.clear(); mPlot->clearAll(); }

void SpectralProfileDialog::exportCsv()
{
    if (mPinned.isEmpty()) { QMessageBox::information(this, tr("Export"), tr("No data.")); return; }
    QString path = QFileDialog::getSaveFileName(this, tr("Export CSV"), QString(), tr("CSV files (*.csv)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export"), tr("Cannot open file.")); return;
    }
    QTextStream ts(&file);
    int nMax = 0;
    for (const auto& p : mPinned) nMax = std::max(nMax, p.bandValues.size());
    ts << "Band";
    for (const auto& p : mPinned) ts << "," << p.layerName << "(" << (int)p.pixelCol << "," << (int)p.pixelRow << ")";
    ts << "\n";
    for (int b = 0; b < nMax; ++b) {
        ts << (b + 1);
        for (const auto& p : mPinned) { ts << ","; if (b < p.bandValues.size()) ts << p.bandValues[b]; }
        ts << "\n";
    }
    file.close();
    QMessageBox::information(this, tr("Export"), tr("Exported to:\n%1").arg(path));
}

void SpectralProfileDialog::closeEvent(QCloseEvent* event) { emit closed(); QDialog::closeEvent(event); }
