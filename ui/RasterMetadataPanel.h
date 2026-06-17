#ifndef RASTERMETADATAPANEL_H
#define RASTERMETADATAPANEL_H

#include <QWidget>

class QLabel;
class QFormLayout;

class RasterMetadataPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RasterMetadataPanel(QWidget* parent = nullptr);

public slots:
    void showMetadata(const QString& layerId, const QString& displayName,
                      int width, int height, int bandCount,
                      const QString& projection, int epsg,
                      double pixelX, double pixelY,
                      const QString& datum, double noData,
                      const QString& dataType, const QString& filePath);
    void clear();

private:
    void setupUI();

    QLabel* mLblDataset;
    QLabel* mLblDimension;
    QLabel* mLblProjection;
    QLabel* mLblPixelSize;
    QLabel* mLblDatum;
    QLabel* mLblNoData;
    QLabel* mLblDataType;
    QLabel* mLblFilePath;
    QLabel* mLblBandCount;
};

#endif // RASTERMETADATAPANEL_H
