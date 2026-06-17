#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include <QDialog>
#include "services/ILayerService.h"

class QComboBox;
class QCheckBox;
class QLabel;

class ExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExportDialog(QWidget* parent = nullptr);

    ExportOptions options() const;

private:
    QComboBox* mCompressionCombo;
    QCheckBox* mPredictorCheck;
    QCheckBox* mPyramidCheck;
    QLabel* mEstimatedSizeLabel;
};

#endif // EXPORTDIALOG_H
