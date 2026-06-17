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

    // Wrap content in a scroll area so small docks remain usable
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outerLayout->addWidget(scroll);

    auto* content = new QWidget(scroll);
    auto* mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    auto* title = new QLabel(QStringLiteral("影像元数据"), content);
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
        auto* lbl = new QLabel(QStringLiteral("--"), content);
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

    form->addRow(QStringLiteral("影像名称:"), mLblDataset);
    form->addRow(QStringLiteral("波段数量:"), mLblBandCount);
    form->addRow(QStringLiteral("图像尺寸:"), mLblDimension);
    form->addRow(QStringLiteral("像元大小:"), mLblPixelSize);
    form->addRow(QStringLiteral("数据类型:"), mLblDataType);
    form->addRow(QStringLiteral("投影信息:"), mLblProjection);
    form->addRow(QStringLiteral("基准面:"),   mLblDatum);
    form->addRow(QStringLiteral("忽略值:"),   mLblNoData);
    form->addRow(QStringLiteral("文件路径:"), mLblFilePath);

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
                                        const QString& dataType, const QString& filePath)
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
}
