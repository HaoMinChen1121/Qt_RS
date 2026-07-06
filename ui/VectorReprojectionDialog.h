#ifndef VECTORREPROJECTIONDIALOG_H
#define VECTORREPROJECTIONDIALOG_H

#include <QDialog>
#include <QgsCoordinateReferenceSystem.h>
#include "domain/params/VectorReprojectionParams.h"

class QLineEdit;
class QLabel;

class VectorReprojectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit VectorReprojectionDialog(QWidget* parent = nullptr);
    void setParams(const VectorReprojectionParams& p);
    VectorReprojectionParams params() const;
private slots:
    void onSelectSource();
    void onSelectCrs();
    void onSelectOutput();
    void onAccepted();
private:
    void setupUI();
    QLineEdit* mSrcPath; QLabel* mSrcInfo;
    QLineEdit* mCrsEdit; QLabel* mCrsDesc;
    QLineEdit* mOutputPath;
    QgsCoordinateReferenceSystem mCrs;
    VectorReprojectionParams mParams;
};

#endif
