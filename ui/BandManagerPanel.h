#ifndef BANDMANAGERPANEL_H
#define BANDMANAGERPANEL_H

#include <QDockWidget>
#include <QComboBox>
#include <QTreeWidget>
#include <QPushButton>
#include "dataaccess/ISensorProduct.h"
#include "domain/SensorInfo.h"
#include "domain/BandConfiguration.h"

struct ProductEntry
{
    QString productId;
    QString productPath;
    QString sensorType;
    QList<RasterBandDescriptor> bands;
    SensorInfo sensorInfo;
};

class BandManagerPanel : public QDockWidget
{
    Q_OBJECT
public:
    explicit BandManagerPanel(QWidget* parent = nullptr);

    void addProduct(const QString& productPath,
                    const QList<RasterBandDescriptor>& bands,
                    const SensorInfo& info);

signals:
    void productOpenRequested(const QString& filePath);
    void applyRgbRequested(const BandConfiguration& cfg,
                           const QString& rPath,
                           const QString& gPath,
                           const QString& bPath,
                           const SensorInfo& info,
                           const QString& productId);

private slots:
    void onBandDoubleClicked(QTreeWidgetItem* item, int column);
    void onProductClicked(QTreeWidgetItem* item, int column);
    void onApply();
    void onOpenProduct();

private:
    void setupUI();
    void rebuildComboItems();
    void updateBandLabels();

    QComboBox* mRedCombo;
    QComboBox* mGreenCombo;
    QComboBox* mBlueCombo;
    QTreeWidget* mProductTree;
    QPushButton* mApplyBtn;

    QList<ProductEntry> mProducts;
};

#endif // BANDMANAGERPANEL_H
