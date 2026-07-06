#ifndef RASTERREPROJECTIONDIALOG_H
#define RASTERREPROJECTIONDIALOG_H

#include <QDialog>
#include <QgsCoordinateReferenceSystem.h>
#include "domain/params/RasterReprojectionParams.h"

class QLineEdit;
class QComboBox;
class QLabel;

class RasterReprojectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RasterReprojectionDialog(QWidget* parent = nullptr);
    void setParams(const RasterReprojectionParams& p);
    RasterReprojectionParams params() const;
private slots:
    void onSelectSource();
    void onSelectCrs();
    void onSelectOutput();
    void onAccepted();
private:
    void setupUI();
    QLineEdit* mSrcPath; QLabel* mSrcInfo;
    QLineEdit* mCrsEdit; QLabel* mCrsDesc;
    QComboBox* mResampleCombo;
    QLineEdit* mOutputPath;
    QgsCoordinateReferenceSystem mCrs;
    RasterReprojectionParams mParams;
};

#endif
