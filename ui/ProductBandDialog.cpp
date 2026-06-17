#include "ProductBandDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDialogButtonBox>

ProductBandDialog::ProductBandDialog(const QList<QPair<QString, QString>>& bands,
                                       QWidget* parent)
    : QDialog(parent)
    , mBands(bands)
    {
    setWindowTitle(tr("产品波段组合"));
    setMinimumWidth(320);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("从产品 %1 个波段中选择 R/G/B 通道:")
        .arg(bands.size())));

    auto* form = new QFormLayout();
    mRed   = new QComboBox();
    mGreen = new QComboBox();
    mBlue  = new QComboBox();

    for (int i = 0; i < bands.size(); ++i)
    {
        QString label = bands[i].second.isEmpty()
            ? tr("波段 %1").arg(i + 1) : bands[i].second;
        mRed->addItem(label, i);
        mGreen->addItem(label, i);
        mBlue->addItem(label, i);
    }

    // 默认: 选前3个波段 (通常对应 B1=B, B2=G, B3=R 或类似)
    if (bands.size() >= 3)
    {
        mRed->setCurrentIndex(2);   // 默认 R=第3波段
        mGreen->setCurrentIndex(1); // 默认 G=第2波段
        mBlue->setCurrentIndex(0);  // 默认 B=第1波段
    }

    form->addRow(tr("Red 通道:"),   mRed);
    form->addRow(tr("Green 通道:"), mGreen);
    form->addRow(tr("Blue 通道:"),  mBlue);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString ProductBandDialog::redLayerId() const
{
    int idx = mRed->currentData().toInt();
    return (idx >= 0 && idx < mBands.size()) ? mBands[idx].first : QString();
}

QString ProductBandDialog::greenLayerId() const
{
    int idx = mGreen->currentData().toInt();
    return (idx >= 0 && idx < mBands.size()) ? mBands[idx].first : QString();
}

QString ProductBandDialog::blueLayerId() const
{
    int idx = mBlue->currentData().toInt();
    return (idx >= 0 && idx < mBands.size()) ? mBands[idx].first : QString();
}
