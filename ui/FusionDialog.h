#ifndef FUSIONDIALOG_H
#define FUSIONDIALOG_H

#include <QDialog>
#include "domain/params/ImageFusionParams.h"

class QComboBox;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;
class QTableWidget;
class QLabel;
class QStackedWidget;
class QGraphicsView;

/**
 * @brief 图像融合参数对话框（纯表示层）
 * @details 提供全色/多光谱输入、融合算法选择与参数配置、质量评价的图形界面。
 */
class FusionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FusionDialog(QWidget* parent = nullptr);

    void setParams(const ImageFusionParams& params);
    ImageFusionParams params() const;

    void setQualityMetrics(const FusionQualityMetrics& metrics);

private slots:
    void onSelectPanImage();
    void onSelectMsImage();
    void onSelectOutputPath();
    void onAlgorithmChanged(const QString& algo);
    void onAccepted();

private:
    void setupUI();
    QWidget* createInputTab();
    QWidget* createAlgorithmTab();
    QWidget* createQualityTab();

    void showIhsParams(bool visible);
    void showBroveyParams(bool visible);
    void showGsParams(bool visible);
    void showPcaParams(bool visible);
    void showHpfParams(bool visible);
    void showWaveletParams(bool visible);

    // Tab1: Input
    QLineEdit* mPanImagePath;
    QLineEdit* mMsImagePath;
    QLineEdit* mOutputPath;

    // Tab2: Algorithm
    QComboBox* mAlgorithmCombo;
    QStackedWidget* mAlgorithmParamsStack;

    // IHS
    QComboBox* mIhsColorModelCombo;
    QComboBox* mIhsStretchCombo;

    // Brovey
    QLineEdit* mBroveyWeightsEdit;

    // GS
    QComboBox* mGsSimMethodCombo;
    QLineEdit* mGsSensorType;

    // PCA
    QSpinBox* mPcaComponentSpin;

    // HPF
    QSpinBox* mHpfKernelSpin;
    QDoubleSpinBox* mHpfWeightSpin;

    // Wavelet
    QSpinBox* mWaveletLevelSpin;
    QComboBox* mWaveletTypeCombo;

    // Tab3: Quality
    QCheckBox* mCheckCorrCoeff;
    QCheckBox* mCheckAvgGradient;
    QCheckBox* mCheckRMSE;
    QCheckBox* mCheckERGAS;
    QCheckBox* mCheckSAM;
    QCheckBox* mCheckSSIM;
    QCheckBox* mCheckUIQI;
    QTableWidget* mQualityTable;
    QLabel* mQualityStatus;
};

#endif // FUSIONDIALOG_H
