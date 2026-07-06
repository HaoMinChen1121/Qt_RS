#ifndef CRSSELECTDIALOG_H
#define CRSSELECTDIALOG_H

#include <QDialog>
#include <QgsCoordinateReferenceSystem.h>

class QgsProjectionSelectionWidget;

class CrsSelectDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CrsSelectDialog(QWidget* parent = nullptr);
    void setCurrentCrs(const QgsCoordinateReferenceSystem& crs);
    QgsCoordinateReferenceSystem selectedCrs() const;
    static QgsCoordinateReferenceSystem selectCrs(QWidget* parent,
        const QgsCoordinateReferenceSystem& current = {}, bool* ok = nullptr);
private:
    void setupUI();
    QgsProjectionSelectionWidget* mCrsWidget;
};

#endif
