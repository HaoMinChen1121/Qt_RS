#include "RasterMetadataPanel.h"

#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

RasterMetadataPanel::RasterMetadataPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void RasterMetadataPanel::setupUI()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outerLayout->addWidget(scroll);

    auto* content = new QWidget(scroll);
    auto* mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    auto* title = new QLabel(QString::fromUtf8("\xe5\xbd\xb1\xe5\x83\x8f\xe5\x85\x83\xe6\x95\xb0\xe6\x8d\xae"), content);
    title->setStyleSheet("font-weight: bold; padding-bottom: 4px;");
    mainLayout->addWidget(title);

    auto* line = new QFrame(content);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setSpacing(8);
    form->setContentsMargins(2, 6, 2, 6);

    auto makeLabel = [content]() {
        auto* lbl = new QLabel(QString::fromUtf8("--"), content);
        lbl->setWordWrap(true);
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        lbl->setMinimumWidth(0);
        return lbl;
    };

    mLblDataset    = makeLabel();
    mLblBandCount  = makeLabel();
    mLblDimension  = makeLabel();
    mLblPixelSize  = makeLabel();
    mLblDataType   = makeLabel();
    mLblProjection = makeLabel();
    mLblDatum      = makeLabel();
    mLblNoData     = makeLabel();
    mLblFilePath   = makeLabel();
    mLblLatLon     = makeLabel();

    form->addRow(QString::fromUtf8("\xe5\xbd\xb1\xe5\x83\x8f\xe5\x90\x8d\xe7\xa7\xb0:"), mLblDataset);
    form->addRow(QString::fromUtf8("\xe6\xb3\xa2\xe6\xae\xb5\xe6\x95\xb0\xe9\x87\x8f:"), mLblBandCount);
    form->addRow(QString::fromUtf8("\xe5\x9b\xbe\xe5\x83\x8f\xe5\xb0\xba\xe5\xaf\xb8:"), mLblDimension);
    form->addRow(QString::fromUtf8("\xe5\x83\x8f\xe5\x85\x83\xe5\xa4\xa7\xe5\xb0\x8f:"), mLblPixelSize);
    form->addRow(QString::fromUtf8("\xe6\x95\xb0\xe6\x8d\xae\xe7\xb1\xbb\xe5\x9e\x8b:"), mLblDataType);
    form->addRow(QString::fromUtf8("\xe6\x8a\x95\xe5\xbd\xb1\xe4\xbf\xa1\xe6\x81\xaf:"), mLblProjection);
    form->addRow(QString::fromUtf8("\xe5\x9f\xba\xe5\x87\x86\xe9\x9d\xa2:"),   mLblDatum);
    form->addRow(QString::fromUtf8("\xe5\xbf\xbd\xe7\x95\xa5\xe5\x80\xbc:"),   mLblNoData);
    form->addRow(QString::fromUtf8("\xe7\xbb\x8f\xe7\xba\xac\xe5\xba\xa6\xe8\x8c\x83\xe5\x9b\xb4 (WGS84):"), mLblLatLon);
    form->addRow(QString::fromUtf8("\xe6\x96\x87\xe4\xbb\xb6\xe8\xb7\xaf\xe5\xbe\x84:"), mLblFilePath);

    mainLayout->addLayout(form);
    mainLayout->addStretch();

    scroll->setWidget(content);
}

void RasterMetadataPanel::showMetadata(const QString& layerId,
                                        const QString& displayName,
                                        int width, int height, int bandCount,
                                        const QString& projection, int epsg,
                                        double pixelX, double pixelY,
                                        const QString& datum, double noData,
                                        const QString& dataType, const QString& filePath,
                                        const QString& latLonDms,
                                        const QString& latLonDecimal)
{
    mLblDataset->setText(displayName.isEmpty() ? layerId : displayName);
    mLblBandCount->setText(QString::number(bandCount));
    mLblDimension->setText(QStringLiteral("%1 × %2").arg(width).arg(height));
    mLblPixelSize->setText(QStringLiteral("%1, %2")
                               .arg(pixelX, 0, 'f', 4)
                               .arg(pixelY, 0, 'f', 4));
    mLblDataType->setText(dataType);

    QString projText;
    if (!projection.isEmpty())
    {
        if (epsg > 0)
            projText = QStringLiteral("EPSG:%1\n%2").arg(epsg).arg(projection);
        else
            projText = projection;
    }
    mLblProjection->setText(projText.isEmpty() ? QStringLiteral("无") : projText);

    mLblDatum->setText(datum.isEmpty() ? QStringLiteral("--") : datum);
    mLblNoData->setText(QString::number(noData));

    if (!latLonDms.isEmpty())
    {
        if (!latLonDecimal.isEmpty())
            mLblLatLon->setText(latLonDms + QStringLiteral("\n") + latLonDecimal);
        else
            mLblLatLon->setText(latLonDms);
    }
    else
    {
        mLblLatLon->setText(QStringLiteral("无法计算 (缺少投影信息)"));
    }

    mLblFilePath->setText(filePath);
}

void RasterMetadataPanel::clear()
{
    mLblDataset->setText(QStringLiteral("--"));
    mLblBandCount->setText(QStringLiteral("--"));
    mLblDimension->setText(QStringLiteral("--"));
    mLblPixelSize->setText(QStringLiteral("--"));
    mLblDataType->setText(QStringLiteral("--"));
    mLblProjection->setText(QStringLiteral("--"));
    mLblDatum->setText(QStringLiteral("--"));
    mLblNoData->setText(QStringLiteral("--"));
    mLblFilePath->setText(QStringLiteral("--"));
    mLblLatLon->setText(QStringLiteral("--"));
}
