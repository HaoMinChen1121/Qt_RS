#ifndef SPECTRALPROFILEDIALOG_H
#define SPECTRALPROFILEDIALOG_H

#include <QDialog>
#include <QVector>
#include <QColor>

class SpectralPlotWidget;

struct SpectralData
{
    QString layerName;
    double  pixelCol = 0, pixelRow = 0;
    double  geoX = 0, geoY = 0;
    QVector<double> bandValues;
    QVector<double> wavelengths;
    QColor  color;
};

class SpectralProfileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SpectralProfileDialog(QWidget* parent = nullptr);

    void addProfile(const SpectralData& data);
    void clearAll();

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUI();
    void exportCsv();

    SpectralPlotWidget* mPlot = nullptr;
    QPushButton* mPinBtn = nullptr;
    QVector<SpectralData> mPinned;
};

#endif // SPECTRALPROFILEDIALOG_H
