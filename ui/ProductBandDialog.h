#ifndef PRODUCTBANDDIALOG_H
#define PRODUCTBANDDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QList>
#include <QPair>
#include <QString>

class ProductBandDialog : public QDialog
{
    Q_OBJECT
public:
    /// @param bands [(layerId, displayName), ...] — 产品下所有单波段图层
    explicit ProductBandDialog(const QList<QPair<QString, QString>>& bands,
                                QWidget* parent = nullptr);

    QString redLayerId()   const;
    QString greenLayerId() const;
    QString blueLayerId()  const;

private:
    QComboBox* mRed;
    QComboBox* mGreen;
    QComboBox* mBlue;
    QList<QPair<QString, QString>> mBands;
};

#endif // PRODUCTBANDDIALOG_H
