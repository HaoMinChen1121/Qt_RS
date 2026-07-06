#include "CrsSelectDialog.h"
#include <qgsprojectionselectionwidget.h>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>

CrsSelectDialog::CrsSelectDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Select CRS"));
    setMinimumSize(500, 400);
    setupUI();
}

void CrsSelectDialog::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    auto* info = new QLabel(tr("Search or browse coordinate reference systems:"), this);
    info->setWordWrap(true);
    layout->addWidget(info);
    mCrsWidget = new QgsProjectionSelectionWidget(this);
    layout->addWidget(mCrsWidget);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CrsSelectDialog::setCurrentCrs(const QgsCoordinateReferenceSystem& crs) { mCrsWidget->setCrs(crs); }

QgsCoordinateReferenceSystem CrsSelectDialog::selectedCrs() const { return mCrsWidget->crs(); }

QgsCoordinateReferenceSystem CrsSelectDialog::selectCrs(QWidget* parent,
    const QgsCoordinateReferenceSystem& current, bool* ok)
{
    CrsSelectDialog dlg(parent);
    if (current.isValid()) dlg.setCurrentCrs(current);
    if (dlg.exec() == QDialog::Accepted) { if (ok) *ok = true; return dlg.selectedCrs(); }
    if (ok) *ok = false;
    return {};
}
