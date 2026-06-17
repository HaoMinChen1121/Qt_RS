#include "BandManagerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>

BandManagerDialog::BandManagerDialog(const QList<RasterBandDescriptor>& bands,
                                     const SensorInfo& info,
                                     QWidget* parent)
    : QDialog(parent)
    , mBands(bands)
    , mSensorInfo(info)
{
    // Defaults: try standard RGB bands; fall back to first 3
    setWindowTitle(QStringLiteral("Band Manager — %1").arg(info.sensorType.isEmpty()
        ? QStringLiteral("Product") : info.sensorType));
    setMinimumSize(480, 320);

    // Detect sensor-specific defaults
    if (info.sensorType.contains("Sentinel", Qt::CaseInsensitive))
    {
        mDefaultRed   = 4;
        mDefaultGreen = 3;
        mDefaultBlue  = 2;
    }
    else if (info.sensorType.contains("Landsat", Qt::CaseInsensitive))
    {
        mDefaultRed   = 4;
        mDefaultGreen = 3;
        mDefaultBlue  = 2;
    }
    else
    {
        // Generic: first 3 bands found, ordered by physical band number
        QList<int> sortedBands;
        for (const auto& b : bands)
            if (b.physicalBand > 0)
                sortedBands.append(b.physicalBand);
        std::sort(sortedBands.begin(), sortedBands.end());
        mDefaultRed   = sortedBands.value(2, 3);
        mDefaultGreen = sortedBands.value(1, 2);
        mDefaultBlue  = sortedBands.value(0, 1);
    }

    setupUI(bands, info);
}

void BandManagerDialog::setupUI(const QList<RasterBandDescriptor>& bands, const SensorInfo& info)
{
    auto* mainLayout = new QVBoxLayout(this);

    // ── Product info ──
    QString infoText = info.sensorType;
    if (!info.sensorId.isEmpty())
        infoText += QStringLiteral(" — %1").arg(info.sensorId);
    mainLayout->addWidget(new QLabel(infoText, this));

    // ── RGB group ──
    auto* rgbGroup = new QGroupBox(QStringLiteral("RGB Composite"), this);
    auto* form = new QFormLayout(rgbGroup);

    mRedCombo   = new QComboBox(rgbGroup);
    mGreenCombo = new QComboBox(rgbGroup);
    mBlueCombo  = new QComboBox(rgbGroup);

    populateCombo(mRedCombo, bands);
    populateCombo(mGreenCombo, bands);
    populateCombo(mBlueCombo, bands);

    // Set defaults
    auto setIfFound = [&](QComboBox* cb, int bandNum) {
        for (int i = 0; i < cb->count(); ++i) {
            if (cb->itemData(i).toInt() == bandNum) {
                cb->setCurrentIndex(i);
                return;
            }
        }
    };
    setIfFound(mRedCombo,   mDefaultRed);
    setIfFound(mGreenCombo, mDefaultGreen);
    setIfFound(mBlueCombo,  mDefaultBlue);

    form->addRow(QStringLiteral("R:"), mRedCombo);
    form->addRow(QStringLiteral("G:"), mGreenCombo);
    form->addRow(QStringLiteral("B:"), mBlueCombo);
    mainLayout->addWidget(rgbGroup);

    // ── Band info labels ──
    auto* infoGroup = new QGroupBox(QStringLiteral("Band Details"), this);
    auto* infoForm = new QFormLayout(infoGroup);
    mRedInfo   = new QLabel(infoGroup);
    mGreenInfo = new QLabel(infoGroup);
    mBlueInfo  = new QLabel(infoGroup);
    infoForm->addRow(QStringLiteral("R:"), mRedInfo);
    infoForm->addRow(QStringLiteral("G:"), mGreenInfo);
    infoForm->addRow(QStringLiteral("B:"), mBlueInfo);
    mainLayout->addWidget(infoGroup);

    onBandChanged(); // update info labels

    // ── Buttons ──
    auto* buttonLayout = new QHBoxLayout();
    auto* resetBtn = new QPushButton(QStringLiteral("Restore Defaults"), this);
    buttonLayout->addWidget(resetBtn);
    buttonLayout->addStretch();
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonLayout->addWidget(buttonBox);
    mainLayout->addLayout(buttonLayout);

    // ── Connections ──
    connect(mRedCombo,   QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BandManagerDialog::onBandChanged);
    connect(mGreenCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BandManagerDialog::onBandChanged);
    connect(mBlueCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BandManagerDialog::onBandChanged);
    connect(resetBtn, &QPushButton::clicked, this, [this, setIfFound]() {
        setIfFound(mRedCombo,   mDefaultRed);
        setIfFound(mGreenCombo, mDefaultGreen);
        setIfFound(mBlueCombo,  mDefaultBlue);
    });
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void BandManagerDialog::populateCombo(QComboBox* combo, const QList<RasterBandDescriptor>& bands)
{
    combo->clear();
    for (const auto& b : bands)
    {
        if (b.physicalBand <= 0) continue;
        QString text = QStringLiteral("B%1 — %2")
            .arg(b.physicalBand).arg(b.bandName);
        if (b.resolution > 0)
            text += QStringLiteral(", %1m").arg((int)b.resolution);
        combo->addItem(text, b.physicalBand);
    }
}

const RasterBandDescriptor* BandManagerDialog::findBand(
    int physBand, const QList<RasterBandDescriptor>& bands) const
{
    for (const auto& b : bands)
        if (b.physicalBand == physBand)
            return &b;
    return nullptr;
}

void BandManagerDialog::updateBandLabel(
    QLabel* label, int physBand, const QList<RasterBandDescriptor>& bands)
{
    const auto* b = findBand(physBand, bands);
    if (!b) { label->setText(QStringLiteral("—")); return; }

    // Get wavelength from SensorInfo
    QString physName = QStringLiteral("B%1").arg(physBand);
    const SensorBandInfo* sbi = nullptr;
    for (const auto& si : mSensorInfo.bands)
    {
        if (si.bandNumber == physBand || si.physicalBand == physName)
        {
            sbi = &si;
            break;
        }
    }

    double wl = 0;
    if (sbi && sbi->wavelengthMin > 0)
        wl = (sbi->wavelengthMin + sbi->wavelengthMax) * 0.5;
    else if (sbi && sbi->wavelengthCentral > 0)
        wl = sbi->wavelengthCentral;

    label->setText(QStringLiteral("%1 (%2nm), %3m")
        .arg(b->bandName)
        .arg(wl > 0 ? QString::number((int)(wl * 1000)) : QStringLiteral("?"))
        .arg(b->resolution > 0 ? QString::number((int)b->resolution) : QStringLiteral("?")));
}

void BandManagerDialog::onBandChanged()
{
    updateBandLabel(mRedInfo,   mRedCombo->currentData().toInt(),   mBands);
    updateBandLabel(mGreenInfo, mGreenCombo->currentData().toInt(), mBands);
    updateBandLabel(mBlueInfo,  mBlueCombo->currentData().toInt(),  mBands);
}

BandConfiguration BandManagerDialog::configuration() const
{
    BandConfiguration cfg;
    cfg.redBand   = mRedCombo->currentData().toInt();
    cfg.greenBand = mGreenCombo->currentData().toInt();
    cfg.blueBand  = mBlueCombo->currentData().toInt();
    return cfg;
}

void BandManagerDialog::setConfiguration(const BandConfiguration& cfg)
{
    for (int i = 0; i < mRedCombo->count(); ++i)
        if (mRedCombo->itemData(i).toInt() == cfg.redBand)
            mRedCombo->setCurrentIndex(i);
    for (int i = 0; i < mGreenCombo->count(); ++i)
        if (mGreenCombo->itemData(i).toInt() == cfg.greenBand)
            mGreenCombo->setCurrentIndex(i);
    for (int i = 0; i < mBlueCombo->count(); ++i)
        if (mBlueCombo->itemData(i).toInt() == cfg.blueBand)
            mBlueCombo->setCurrentIndex(i);
}
