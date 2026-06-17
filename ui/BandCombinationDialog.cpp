#include "BandCombinationDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>

BandCombinationDialog::BandCombinationDialog(int bandCount,
                                               const QString& sensorType,
                                               QWidget* parent)
    : QDialog(parent)
    , mBandCount(bandCount)
    {
    setWindowTitle(tr("波段组合"));
    setMinimumWidth(280);

    auto* mainLayout = new QVBoxLayout(this);

    auto* infoLabel = new QLabel(tr("波段总数: %1").arg(bandCount));
    mainLayout->addWidget(infoLabel);

    auto* form = new QFormLayout();
    mRed   = new QSpinBox(); mRed->setRange(1, bandCount);  mRed->setValue(1);
    mGreen = new QSpinBox(); mGreen->setRange(1, bandCount); mGreen->setValue(2);
    mBlue  = new QSpinBox(); mBlue->setRange(1, bandCount);  mBlue->setValue(3);
    form->addRow(tr("Red 波段:"),   mRed);
    form->addRow(tr("Green 波段:"), mGreen);
    form->addRow(tr("Blue 波段:"),  mBlue);
    mainLayout->addLayout(form);

    mPresets = new QComboBox();
    mPresets->addItem(tr("-- 预设 --"));
    addPresets(sensorType);
    connect(mPresets, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BandCombinationDialog::onPresetChanged);
    mainLayout->addWidget(mPresets);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

int BandCombinationDialog::redBand()   const { return mRed->value(); }
int BandCombinationDialog::greenBand() const { return mGreen->value(); }
int BandCombinationDialog::blueBand()  const { return mBlue->value(); }

void BandCombinationDialog::onPresetChanged(int index)
{
    if (index <= 0) return;
    QStringList parts = mPresets->currentData().toString().split('-');
    if (parts.size() == 3)
    {
        mRed->setValue(parts[0].toInt());
        mGreen->setValue(parts[1].toInt());
        mBlue->setValue(parts[2].toInt());
    }
}

void BandCombinationDialog::addPresets(const QString& sensorType)
{
    // 通用预设
    mPresets->addItem(tr("真彩色 3-2-1"),     QStringLiteral("3-2-1"));
    mPresets->addItem(tr("标准假彩色 4-3-2"), QStringLiteral("4-3-2"));

    // 传感器专属预设
    if (sensorType.contains("Sentinel", Qt::CaseInsensitive))
    {
        mPresets->addItem(tr("S2 真彩色 4-3-2"),    QStringLiteral("4-3-2"));
        mPresets->addItem(tr("S2 假彩色 8-4-3"),    QStringLiteral("8-4-3"));
        mPresets->addItem(tr("S2 植被分析 11-8-4"), QStringLiteral("11-8-4"));
        mPresets->addItem(tr("S2 近红外 8A-5-2"),   QStringLiteral("9-5-2"));
    }
else if (sensorType.contains("Landsat", Qt::CaseInsensitive))
{
    mPresets->addItem(tr("LS 真彩色 4-3-2"),    QStringLiteral("4-3-2"));
        mPresets->addItem(tr("LS 假彩色 5-4-3"),    QStringLiteral("5-4-3"));
        mPresets->addItem(tr("LS 红外增强 7-5-3"),  QStringLiteral("7-5-3"));
    }
}
