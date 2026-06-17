#ifndef RADIOMETRICDIALOG_H
#define RADIOMETRICDIALOG_H

#include <QDialog>
#include "domain/params/RadiometricCorrectionParams.h"
#include "domain/SensorInfo.h"

class QComboBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QLabel;

/**
 * @brief 辐射定标与大气校正参数对话框（纯表示层）
 * @details 提供传感器选择、标定参数、大气校正模型配置的图形界面。
 *          不包含任何业务逻辑或数据访问代码。
 */
class RadiometricDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RadiometricDialog(QWidget* parent = nullptr);

    void setParams(const RadiometricCorrectionParams& params);
    void setSensorInfo(const SensorInfo& info);
    RadiometricCorrectionParams params() const;

private slots:
    void onAddInputFile();
    void onRemoveInputFile();
    void onSelectOutputDir();
    void onSelectMetadata();
    void onSensorChanged(const QString& sensor);
    void onAtmModelChanged(const QString& model);
    void onAccepted();

private:
    void setupUI();
    QWidget* createInputTab();
    QWidget* createCalibrationTab();
    QWidget* createAtmosphericTab();
    QWidget* createOutputTab();

    // Tab1: Input
    QComboBox* mSensorCombo;
    QListWidget* mInputFileList;
    QLineEdit* mMetadataPath;
    QLineEdit* mOutputDir;

    // Tab2: Calibration
    QComboBox* mCalibrationTypeCombo;
    QRadioButton* mAutoGainRadio;
    QRadioButton* mManualGainRadio;
    QDoubleSpinBox* mManualGainSpin;
    QDoubleSpinBox* mManualOffsetSpin;
    QDoubleSpinBox* mSolarZenithSpin;
    QDoubleSpinBox* mSolarAzimuthSpin;
    QDoubleSpinBox* mEarthSunDistSpin;
    QDoubleSpinBox* mSensorZenithSpin;
    QDoubleSpinBox* mSensorAzimuthSpin;
    QComboBox* mOutputDataTypeCombo;

    // Tab3: Atmospheric
    QComboBox* mAtmModelCombo;
    QComboBox* mAerosolModelCombo;
    QComboBox* mAtmosphericModelCombo;
    QDoubleSpinBox* mAot550Spin;
    QDoubleSpinBox* mWaterVaporSpin;
    QDoubleSpinBox* mOzoneSpin;
    QDoubleSpinBox* mTargetElevSpin;
    QDoubleSpinBox* mSensorAltSpin;
    QSpinBox* mSen2corResSpin;
    QWidget* mSen2corGroup;
    QWidget* mSixsGroup;

    // Tab4: Output
    QComboBox* mOutputFormatCombo;
    QDoubleSpinBox* mScaleFactorSpin;
    QLineEdit* mNamingPattern;
    QCheckBox* mBatchModeCheck;

    QLabel* mStatusLabel;

    // Read-only metadata display
    QLabel* mAcqTimeLabel;
    QLabel* mQuantValueLabel;
    QLabel* mAotRefLabel;
    QLabel* mWvRefLabel;
    SensorInfo mDisplaySensorInfo;
};

#endif // RADIOMETRICDIALOG_H
