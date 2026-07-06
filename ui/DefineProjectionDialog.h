#ifndef DEFINEPROJECTIONDIALOG_H
#define DEFINEPROJECTIONDIALOG_H

#include <QDialog>
#include <QgsCoordinateReferenceSystem.h>
#include "domain/params/DefineProjectionParams.h"

class QLineEdit;
class QLabel;

class DefineProjectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DefineProjectionDialog(QWidget* parent = nullptr);
    void setParams(const DefineProjectionParams& p);
    DefineProjectionParams params() const;
private slots:
    void onSelectSource();
    void onSelectCrs();
    void onAccepted();
private:
    void setupUI();
    QLineEdit* mSrcPath; QLabel* mSrcInfo;
    QLineEdit* mCrsEdit; QLabel* mCrsDesc;
    QgsCoordinateReferenceSystem mCrs;
    DefineProjectionParams mParams;
};

#endif
