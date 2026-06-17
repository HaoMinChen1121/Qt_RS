#ifndef GEOMETRICDIALOG_H
#define GEOMETRICDIALOG_H

#include <QDialog>
#include "GeometricTypes.h"

class QComboBox;
class QLineEdit;
class QTableWidget;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;
class QLabel;

class GeometricDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GeometricDialog(QWidget* parent = nullptr);

    void setInput(const GeometricInput& input);
    GeometricInput inputParams() const;

private slots:
    void onSelectSourceImage();
    void onSelectReferenceImage();
    void onSelectOutputPath();
    void onAddGcp();
    void onDeleteGcp();
    void onImportGcp();
    void onExportGcp();
    void onAutoDetect();
    void onAccepted();

private:
    void setupUI();
    QWidget* createInputTab();
    QWidget* createGcpTab();
    QWidget* createModelTab();

    // Tab 1: Input
    QLineEdit* mSrcPath;
    QLineEdit* mRefPath;
    QComboBox* mRefTypeCombo;
    QLineEdit* mOutputPath;

    // Tab 2: GCP List
    QTableWidget* mGcpTable;
    QLabel* mLblGcpCount;
    QLabel* mLblRms;

    // Tab 3: Model
    QComboBox* mModelCombo;
    QSpinBox* mPolyOrderSpin;
    QComboBox* mResampleCombo;
    QLineEdit* mOutProj;
    QDoubleSpinBox* mPxSizeX;
    QDoubleSpinBox* mPxSizeY;
    QDoubleSpinBox* mExtMinX;
    QDoubleSpinBox* mExtMinY;
    QDoubleSpinBox* mExtMaxX;
    QDoubleSpinBox* mExtMaxY;

    GeometricInput mInput;
};

#endif // GEOMETRICDIALOG_H
