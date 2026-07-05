#ifndef VECTORMETADATAPANEL_H
#define VECTORMETADATAPANEL_H

#include <QWidget>

struct VectorLayerInfo;

class QLabel;

class VectorMetadataPanel : public QWidget
{
    Q_OBJECT

public:
    explicit VectorMetadataPanel(QWidget* parent = nullptr);

public slots:
    void showMetadata(const VectorLayerInfo& info, const QString& datum);
    void clear();

private:
    void setupUI();

    QLabel* mLblDataset;
    QLabel* mLblGeomType;
    QLabel* mLblFeatureCount;
    QLabel* mLblProjection;
    QLabel* mLblDatum;
    QLabel* mLblFields;
    QLabel* mLblExtent;
    QLabel* mLblFilePath;
};

#endif // VECTORMETADATAPANEL_H
