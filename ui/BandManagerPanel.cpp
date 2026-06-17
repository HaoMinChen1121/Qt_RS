#include "BandManagerPanel.h"
#include "dataaccess/SensorProductFactory.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QHeaderView>
#include <QDebug>

BandManagerPanel::BandManagerPanel(QWidget* parent)
    : QDockWidget(QStringLiteral("Band Manager"), parent)
{
    setWindowTitle(QStringLiteral("Band Manager"));
    setMinimumSize(320, 400);
    setupUI();
}

void BandManagerPanel::setupUI()
{
    auto* mainWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    // ── Product tree (top) ──
    auto* treeGroup = new QGroupBox(QStringLiteral("Loaded Products"), mainWidget);
    auto* treeLayout = new QVBoxLayout(treeGroup);

    QPushButton* openBtn = new QPushButton(QStringLiteral("+ Open Product"), treeGroup);
    treeLayout->addWidget(openBtn);

    mProductTree = new QTreeWidget(treeGroup);
    mProductTree->setHeaderLabels({
        QStringLiteral("Band / Product"),
        QStringLiteral("Wavelength"),
        QStringLiteral("Resolution")
    });
    mProductTree->setRootIsDecorated(true);
    mProductTree->setAlternatingRowColors(true);
    mProductTree->header()->setStretchLastSection(false);
    mProductTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    mProductTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    mProductTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    treeLayout->addWidget(mProductTree);
    mainLayout->addWidget(treeGroup);

    // ── RGB assignment (bottom) ──
    auto* rgbGroup = new QGroupBox(QStringLiteral("RGB Channels"), mainWidget);
    auto* rgbForm = new QFormLayout(rgbGroup);

    mRedCombo   = new QComboBox(rgbGroup);
    mGreenCombo = new QComboBox(rgbGroup);
    mBlueCombo  = new QComboBox(rgbGroup);

    rgbForm->addRow(QStringLiteral("R:"), mRedCombo);
    rgbForm->addRow(QStringLiteral("G:"), mGreenCombo);
    rgbForm->addRow(QStringLiteral("B:"), mBlueCombo);
    mainLayout->addWidget(rgbGroup);

    mApplyBtn = new QPushButton(QStringLiteral("Apply to Canvas"), mainWidget);
    mainLayout->addWidget(mApplyBtn);

    setWidget(mainWidget);

    // ── Connections ──
    connect(mApplyBtn, &QPushButton::clicked, this, &BandManagerPanel::onApply);
    connect(openBtn, &QPushButton::clicked, this, &BandManagerPanel::onOpenProduct);
    connect(mProductTree, &QTreeWidget::itemDoubleClicked,
            this, &BandManagerPanel::onBandDoubleClicked);
    connect(mProductTree, &QTreeWidget::itemClicked,
            this, &BandManagerPanel::onProductClicked);
    connect(mRedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateBandLabels(); });
    connect(mGreenCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateBandLabels(); });
    connect(mBlueCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateBandLabels(); });
}

void BandManagerPanel::addProduct(const QString& productPath,
                                   const QList<RasterBandDescriptor>& bands,
                                   const SensorInfo& info)
{
    ProductEntry entry;
    entry.productPath = productPath;

    // Generate a display ID — include tile ID when available to distinguish
    // products from the same date but different tiles (e.g. Sentinel-2 T49RFN/T49RGN)
    QString dateStr = info.acquisitionTime.toString(QStringLiteral("yyyyMMdd"));
    if (info.sensorType.isEmpty())
    {
        entry.productId = QFileInfo(productPath).fileName();
    }
    else if (!info.tileId.isEmpty() && !dateStr.isEmpty())
    {
        entry.productId = QStringLiteral("%1_%2_%3").arg(info.sensorType, dateStr, info.tileId);
    }
    else
    {
        entry.productId = QStringLiteral("%1_%2").arg(info.sensorType, dateStr);
    }

    entry.sensorType = info.sensorType;
    entry.bands = bands;
    entry.sensorInfo = info;

    mProducts.append(entry);

    // Add to tree
    auto* productItem = new QTreeWidgetItem(mProductTree);
    productItem->setText(0, entry.productId);
    productItem->setData(0, Qt::UserRole, mProducts.size() - 1);  // index into mProducts
    productItem->setFlags(productItem->flags() | Qt::ItemIsAutoTristate);

    for (const auto& b : bands)
    {
        if (b.physicalBand <= 0) continue;

        auto* bandItem = new QTreeWidgetItem(productItem);
        bandItem->setText(0, QStringLiteral("B%1 — %2")
            .arg(b.physicalBand).arg(b.bandName));

        double wl = 0;
        for (const auto& sbi : info.bands)
        {
            if (sbi.bandNumber == b.physicalBand)
            {
                wl = sbi.wavelengthCentral > 0
                    ? sbi.wavelengthCentral
                    : (sbi.wavelengthMin + sbi.wavelengthMax) * 0.5;
                break;
            }
        }
        bandItem->setText(1, wl > 0
            ? QStringLiteral("%1 nm").arg((int)(wl * 1000))
            : QStringLiteral("—"));
        bandItem->setText(2, b.resolution > 0
            ? QStringLiteral("%1 m").arg((int)b.resolution)
            : QStringLiteral("—"));

        // Store product index + band index for lookup
        int data = (mProducts.size() - 1) << 16 | b.physicalBand;
        bandItem->setData(0, Qt::UserRole, data);
    }

    productItem->setExpanded(true);

    // Rebuild combo items (now includes new product's bands)
    rebuildComboItems();

    // Auto-assign defaults if this is the first product
    if (mProducts.size() == 1)
    {
        // Find R=4, G=3, B=2 (Sentinel-2/Landsat default) or first 3 bands
        auto setCombo = [&](QComboBox* cb, int target) {
            for (int i = 0; i < cb->count(); ++i) {
                int bandNum = cb->itemData(i).toInt() & 0xFFFF;
                int prodIdx = (cb->itemData(i).toInt() >> 16) & 0xFFFF;
                if (bandNum == target && prodIdx == 0) {
                    cb->setCurrentIndex(i);
                    return;
                }
            }
        };
        setCombo(mRedCombo, 4);
        setCombo(mGreenCombo, 3);
        setCombo(mBlueCombo, 2);
    }
}

void BandManagerPanel::rebuildComboItems()
{
    mRedCombo->blockSignals(true);
    mGreenCombo->blockSignals(true);
    mBlueCombo->blockSignals(true);

    mRedCombo->clear();
    mGreenCombo->clear();
    mBlueCombo->clear();

    for (int p = 0; p < mProducts.size(); ++p)
    {
        const auto& prod = mProducts[p];
        for (const auto& b : prod.bands)
        {
            if (b.physicalBand <= 0) continue;

            double wl = 0;
            for (const auto& sbi : prod.sensorInfo.bands)
            {
                if (sbi.bandNumber == b.physicalBand)
                {
                    wl = sbi.wavelengthCentral > 0
                        ? sbi.wavelengthCentral
                        : (sbi.wavelengthMin + sbi.wavelengthMax) * 0.5;
                    break;
                }
            }

            QString text = QStringLiteral("%1 — B%2 %3")
                .arg(prod.productId)
                .arg(b.physicalBand)
                .arg(b.bandName);
            if (wl > 0)
                text += QStringLiteral(" (%1nm)").arg((int)(wl * 1000));

            // Encode: upper 16 bits = product index, lower 16 bits = physical band
            int data = (p << 16) | (b.physicalBand & 0xFFFF);

            mRedCombo->addItem(text, data);
            mGreenCombo->addItem(text, data);
            mBlueCombo->addItem(text, data);
        }
    }

    mRedCombo->blockSignals(false);
    mGreenCombo->blockSignals(false);
    mBlueCombo->blockSignals(false);
}

void BandManagerPanel::onBandDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item || item->parent() == nullptr) return; // product node, not band

    int data = item->data(0, Qt::UserRole).toInt();
    int bandNum = data & 0xFFFF;

    // Find the next "free" combo and assign
    // If none assigned yet, assign R first, then G, then B
    if (mRedCombo->currentIndex() < 0 || mRedCombo->currentData().toInt() == 0)
        mRedCombo->setCurrentIndex(mRedCombo->findData(data));
    else if (mGreenCombo->currentIndex() < 0 || mGreenCombo->currentData().toInt() == 0)
        mGreenCombo->setCurrentIndex(mGreenCombo->findData(data));
    else
        mBlueCombo->setCurrentIndex(mBlueCombo->findData(data));

    updateBandLabels();
}

void BandManagerPanel::onProductClicked(QTreeWidgetItem* item, int /*column*/)
{
    // When a product row is clicked, auto-fill R/G/B with defaults from that product
    if (!item || item->parent() != nullptr) return; // band node, not product

    int prodIdx = item->data(0, Qt::UserRole).toInt();
    if (prodIdx < 0 || prodIdx >= mProducts.size()) return;

    const auto& prod = mProducts[prodIdx];

    // Find R=4, G=3, B=2, or fallback to first 3 bands
    auto setCombo = [&](QComboBox* cb, int target) {
        for (int i = 0; i < cb->count(); ++i) {
            int bandNum = cb->itemData(i).toInt() & 0xFFFF;
            int pIdx = (cb->itemData(i).toInt() >> 16) & 0xFFFF;
            if (bandNum == target && pIdx == prodIdx) {
                cb->setCurrentIndex(i);
                return;
            }
        }
        // Fallback: first band of this product
        for (int i = 0; i < cb->count(); ++i) {
            int pIdx = (cb->itemData(i).toInt() >> 16) & 0xFFFF;
            if (pIdx == prodIdx) {
                cb->setCurrentIndex(i);
                return;
            }
        }
    };
    setCombo(mRedCombo, 4);
    setCombo(mGreenCombo, 3);
    setCombo(mBlueCombo, 2);
    updateBandLabels();
}

void BandManagerPanel::updateBandLabels()
{
    // Visual feedback: highlight which bands are assigned in the tree
    int rData = mRedCombo->currentData().toInt();
    int gData = mGreenCombo->currentData().toInt();
    int bData = mBlueCombo->currentData().toInt();

    for (int i = 0; i < mProductTree->topLevelItemCount(); ++i)
    {
        auto* prodItem = mProductTree->topLevelItem(i);
        for (int j = 0; j < prodItem->childCount(); ++j)
        {
            auto* bandItem = prodItem->child(j);
            int bData2 = bandItem->data(0, Qt::UserRole).toInt();
            QString marker;
            if (bData2 == rData) marker = QStringLiteral("  ◀ R");
            else if (bData2 == gData) marker = QStringLiteral("  ◀ G");
            else if (bData2 == bData) marker = QStringLiteral("  ◀ B");

            // Restore base text (strip previous marker)
            QString base = bandItem->text(0);
            int markerPos = base.indexOf(QStringLiteral("  ◀"));
            if (markerPos >= 0)
                base = base.left(markerPos);
            bandItem->setText(0, base + marker);
        }
    }
}

void BandManagerPanel::onApply()
{
    int rData = mRedCombo->currentData().toInt();
    int gData = mGreenCombo->currentData().toInt();
    int bData = mBlueCombo->currentData().toInt();

    int rProdIdx = (rData >> 16) & 0xFFFF;
    int gProdIdx = (gData >> 16) & 0xFFFF;
    int bProdIdx = (bData >> 16) & 0xFFFF;

    if (rProdIdx >= mProducts.size() || gProdIdx >= mProducts.size()
        || bProdIdx >= mProducts.size())
        return;

    int rBand = rData & 0xFFFF;
    int gBand = gData & 0xFFFF;
    int bBand = bData & 0xFFFF;

    // Find raster paths
    auto findPath = [&](int prodIdx, int bandNum) -> QString {
        for (const auto& b : mProducts[prodIdx].bands)
            if (b.physicalBand == bandNum)
                return b.rasterPath;
        return {};
    };

    QString rPath = findPath(rProdIdx, rBand);
    QString gPath = findPath(gProdIdx, gBand);
    QString bPath = findPath(bProdIdx, bBand);
    if (rPath.isEmpty() || gPath.isEmpty() || bPath.isEmpty())
        return;

    BandConfiguration cfg;
    cfg.productId = mProducts[rProdIdx].productId;
    cfg.redBand   = rBand;
    cfg.greenBand = gBand;
    cfg.blueBand  = bBand;

    emit applyRgbRequested(cfg, rPath, gPath, bPath,
                           mProducts[rProdIdx].sensorInfo,
                           mProducts[rProdIdx].productId);
}

void BandManagerPanel::onOpenProduct()
{
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open Product"),
        QString(),
        QStringLiteral("All Supported (*.zip *.SAFE *.rpp *.tif *.tiff);;All Files (*.*)")
    );
    if (!path.isEmpty())
        emit productOpenRequested(path);
}
