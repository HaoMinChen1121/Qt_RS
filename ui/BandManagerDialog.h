#ifndef BANDMANAGERDIALOG_H
#define BANDMANAGERDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include "dataaccess/ISensorProduct.h"
#include "domain/SensorInfo.h"
#include "domain/BandConfiguration.h"

class BandManagerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BandManagerDialog(const QList<RasterBandDescriptor>& bands,
                               const SensorInfo& info,
                               QWidget* parent = nullptr);

    BandConfiguration configuration() const;

    // Populate from existing config (for reconfiguration)
    void setConfiguration(const BandConfiguration& cfg);

private slots:
    void onBandChanged();

private:
    void setupUI(const QList<RasterBandDescriptor>& bands, const SensorInfo& info);
    void populateCombo(QComboBox* combo, const QList<RasterBandDescriptor>& bands);
    void updateBandLabel(QLabel* label, int physBand, const QList<RasterBandDescriptor>& bands);
    const RasterBandDescriptor* findBand(int physBand, const QList<RasterBandDescriptor>& bands) const;

    QComboBox* mRedCombo;
    QComboBox* mGreenCombo;
    QComboBox* mBlueCombo;

    QLabel* mRedInfo;
    QLabel* mGreenInfo;
    QLabel* mBlueInfo;

    QList<RasterBandDescriptor> mBands;
    SensorInfo mSensorInfo;
    int mDefaultRed;
    int mDefaultGreen;
    int mDefaultBlue;
};

#endif // BANDMANAGERDIALOG_H
