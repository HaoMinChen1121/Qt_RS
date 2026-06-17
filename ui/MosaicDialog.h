#ifndef MOSAICDIALOG_H
#define MOSAICDIALOG_H

#include <QDialog>
#include "domain/params/MosaicParams.h"

class QComboBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QLabel;
class QGroupBox;

/**
 * @brief 影像镶嵌与成图参数对话框（纯表示层）
 * @details 提供多景影像匀色、拼接线生成、羽化融合、输出设置的图形界面。
 */
class MosaicDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MosaicDialog(QWidget* parent = nullptr);

    void setParams(const MosaicParams& params);
    MosaicParams params() const;

private slots:
    void onAddImages();
    void onRemoveImages();
    void onMoveImageUp();
    void onMoveImageDown();
    void onSelectHistRefImage();
    void onSelectOutputPath();
    void onColorBalanceMethodChanged(const QString& method);
    void onAccepted();

private:
    void setupUI();
    QWidget* createInputTab();
    QWidget* createColorBalanceTab();
    QWidget* createSeamlineTab();
    QWidget* createOutputTab();

    // Tab1: Input
    QListWidget* mImageList;
    QLabel* mImageInfoLabel;

    // Tab2: Color Balance
    QComboBox* mColorBalanceCombo;
    QLineEdit* mHistRefImagePath;
    QSpinBox* mWallisWindowSpin;
    QDoubleSpinBox* mWallisContrastSpin;
    QDoubleSpinBox* mWallisBrightnessSpin;
    QWidget* mWallisGroup;

    // Tab3: Seamline
    QComboBox* mSeamlineMethodCombo;
    QDoubleSpinBox* mEdgeWeightSpin;
    QDoubleSpinBox* mColorWeightSpin;
    QDoubleSpinBox* mTextureWeightSpin;
    QWidget* mSeamlineParamsGroup;

    // Tab4: Output
    QCheckBox* mUseImageExtentCheck;
    QDoubleSpinBox* mExtentMinXSpin;
    QDoubleSpinBox* mExtentMinYSpin;
    QDoubleSpinBox* mExtentMaxXSpin;
    QDoubleSpinBox* mExtentMaxYSpin;
    QLineEdit* mOutputProjection;
    QDoubleSpinBox* mResXSpin;
    QDoubleSpinBox* mResYSpin;
    QComboBox* mOutputFormatCombo;
    QSpinBox* mFeatheringWidthSpin;
    QComboBox* mFeatheringTypeCombo;
    QSpinBox* mBackgroundValueSpin;
    QSpinBox* mBlockSizeSpin;
    QLineEdit* mOutputPath;

    QLabel* mStatusLabel;
};

#endif // MOSAICDIALOG_H
