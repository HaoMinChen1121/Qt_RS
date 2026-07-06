#include "VectorMetadataPanel.h"
#include "domain/VectorLayerInfo.h"

#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

VectorMetadataPanel::VectorMetadataPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void VectorMetadataPanel::setupUI()
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

    auto* title = new QLabel(QString::fromUtf8("\xe7\x9f\xa2\xe9\x87\x8f\xe5\x85\x83\xe6\x95\xb0\xe6\x8d\xae"), content);
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

    mLblDataset      = makeLabel();
    mLblGeomType     = makeLabel();
    mLblFeatureCount = makeLabel();
    mLblProjection   = makeLabel();
    mLblDatum        = makeLabel();
    mLblFields       = makeLabel();
    mLblExtent       = makeLabel();
    mLblFilePath     = makeLabel();

    form->addRow(QString::fromUtf8("\xe5\x9b\xbe\xe5\xb1\x82\xe5\x90\x8d\xe7\xa7\xb0:"), mLblDataset);
    form->addRow(QString::fromUtf8("\xe5\x87\xa0\xe4\xbd\x95\xe7\xb1\xbb\xe5\x9e\x8b:"), mLblGeomType);
    form->addRow(QString::fromUtf8("\xe8\xa6\x81\xe7\xb4\xa0\xe6\x95\xb0\xe9\x87\x8f:"), mLblFeatureCount);
    form->addRow(QString::fromUtf8("\xe6\x8a\x95\xe5\xbd\xb1\xe4\xbf\xa1\xe6\x81\xaf:"), mLblProjection);
    form->addRow(QString::fromUtf8("\xe5\x9f\xba\xe5\x87\x86\xe9\x9d\xa2:"),   mLblDatum);
    form->addRow(QString::fromUtf8("\xe5\xb1\x9e\xe6\x80\xa7\xe5\xad\x97\xe6\xae\xb5:"), mLblFields);
    form->addRow(QString::fromUtf8("\xe7\xa9\xba\xe9\x97\xb4\xe8\x8c\x83\xe5\x9b\xb4:"), mLblExtent);
    form->addRow(QString::fromUtf8("\xe6\x96\x87\xe4\xbb\xb6\xe8\xb7\xaf\xe5\xbe\x84:"), mLblFilePath);

    mainLayout->addLayout(form);
    mainLayout->addStretch();

    scroll->setWidget(content);
}

void VectorMetadataPanel::showMetadata(const VectorLayerInfo& info, const QString& datum)
{
    mLblDataset->setText(info.displayName.isEmpty() ? info.layerId : info.displayName);
    mLblGeomType->setText(info.geometryType);
    mLblFeatureCount->setText(QString::number(info.featureCount));

    QString projText;
    if (!info.projectionWkt.isEmpty())
    {
        if (info.epsgCode > 0)
            projText = QStringLiteral("EPSG:%1\n%2").arg(info.epsgCode).arg(info.projectionWkt);
        else
            projText = info.projectionWkt;
    }
    mLblProjection->setText(projText.isEmpty() ? QStringLiteral("无") : projText);

    mLblDatum->setText(datum.isEmpty() ? QStringLiteral("--") : datum);

    // Fields: show name(type) pairs
    if (!info.fieldNames.isEmpty())
    {
        QStringList fieldDescs;
        for (int i = 0; i < info.fieldNames.size(); ++i)
        {
            const QString type = i < info.fieldTypes.size() ? info.fieldTypes[i] : QStringLiteral("?");
            fieldDescs << QStringLiteral("%1 (%2)").arg(info.fieldNames[i], type);
        }
        mLblFields->setText(fieldDescs.join(QStringLiteral("\n")));
    }
    else
    {
        mLblFields->setText(QStringLiteral("--"));
    }

    // Extent
    if (!info.extent.isNull() && info.extent.width() > 0)
    {
        mLblExtent->setText(QStringLiteral("(%1, %2) ~ (%3, %4)")
            .arg(info.extent.left(), 0, 'f', 6)
            .arg(info.extent.bottom(), 0, 'f', 6)
            .arg(info.extent.right(), 0, 'f', 6)
            .arg(info.extent.top(), 0, 'f', 6));
    }
    else
    {
        mLblExtent->setText(QStringLiteral("--"));
    }

    mLblFilePath->setText(info.filePath);
}

void VectorMetadataPanel::clear()
{
    mLblDataset->setText(QStringLiteral("--"));
    mLblGeomType->setText(QStringLiteral("--"));
    mLblFeatureCount->setText(QStringLiteral("--"));
    mLblProjection->setText(QStringLiteral("--"));
    mLblDatum->setText(QStringLiteral("--"));
    mLblFields->setText(QStringLiteral("--"));
    mLblExtent->setText(QStringLiteral("--"));
    mLblFilePath->setText(QStringLiteral("--"));
}
