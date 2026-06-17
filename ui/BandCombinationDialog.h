#ifndef BANDCOMBINATIONDIALOG_H
#define BANDCOMBINATIONDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QString>
#include <QStringList>

class BandCombinationDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BandCombinationDialog(int bandCount,
                                    const QString& sensorType = {},
                                    QWidget* parent = nullptr);

    int redBand()   const;
    int greenBand() const;
    int blueBand()  const;

private slots:
    void onPresetChanged(int index);

private:
    void addPresets(const QString& sensorType);

    int mBandCount;
    QSpinBox* mRed;
    QSpinBox* mGreen;
    QSpinBox* mBlue;
    QComboBox* mPresets;
};

#endif // BANDCOMBINATIONDIALOG_H
