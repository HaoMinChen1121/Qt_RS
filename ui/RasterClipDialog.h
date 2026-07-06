#ifndef RASTERCLIPDIALOG_H
#define RASTERCLIPDIALOG_H

#include <QDialog>
#include "domain/params/RasterClipParams.h"

class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;

class RasterClipDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RasterClipDialog(QWidget* parent = nullptr);

    void setParams(const RasterClipParams& params);
    RasterClipParams params() const;

private slots:
    void onSelectRaster();
    void onSelectVector();
    void onSelectOutput();
    void onAccepted();

private:
    void setupUI();
    void refreshLayerList();

    QLineEdit* mRasterPath;
    QLabel* mRasterInfo;
    QLineEdit* mVectorPath;
    QComboBox* mLayerCombo;
    QCheckBox* mCropCheck;
    QLineEdit* mOutputPath;
    QLabel* mVectorInfo;

    RasterClipParams mParams;
};

#endif // RASTERCLIPDIALOG_H
