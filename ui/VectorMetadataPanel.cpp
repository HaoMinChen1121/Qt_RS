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

    auto* title = new QLabel(QStringLiteral("矢量元数据"), content);
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

    mLblDataset      = makeLabel();
    mLblGeomType     = makeLabel();
    mLblFeatureCount = makeLabel();
    mLblProjection   = makeLabel();
    mLblDatum        = makeLabel();
    mLblFields       = makeLabel();
    mLblExtent       = makeLabel();
    mLblFilePath     = makeLabel();

    form->addRow(QStringLiteral("图层名称:"), mLblDataset);
    form->addRow(QStringLiteral("几何类型:"), mLblGeomType);
    form->addRow(QStringLiteral("要素数量:"), mLblFeatureCount);
    form->addRow(QStringLiteral("投影信息:"), mLblProjection);
    form->addRow(QStringLiteral("基准面:"),   mLblDatum);
    form->addRow(QStringLiteral("属性字段:"), mLblFields);
    form->addRow(QStringLiteral("空间范围:"), mLblExtent);
    form->addRow(QStringLiteral("文件路径:"), mLblFilePath);

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
