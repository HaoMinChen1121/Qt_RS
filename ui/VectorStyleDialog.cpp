#include "VectorStyleDialog.h"

#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QColorDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPainter>
#include <QMouseEvent>

// ═══════════════════════════════════════════════════════════════════════
// ColorSwatch — clickable rounded color preview
// ═══════════════════════════════════════════════════════════════════════
ColorSwatch::ColorSwatch(QWidget* parent)
    : QWidget(parent), mColor(Qt::gray)
{
    setFixedSize(36, 24);
    setCursor(Qt::PointingHandCursor);
}

void ColorSwatch::setColor(const QColor& c) { mColor = c; update(); }

void ColorSwatch::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(160, 160, 160), 1));
    p.setBrush(mColor);
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
}

void ColorSwatch::mousePressEvent(QMouseEvent*) { emit clicked(); }

// ═══════════════════════════════════════════════════════════════════════
// ColorRampBar — horizontal gradient preview
// ═══════════════════════════════════════════════════════════════════════
ColorRampBar::ColorRampBar(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(28);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ColorRampBar::setClassCount(int count) { mClassCount = qMax(count, 2); update(); }
void ColorRampBar::setBaseHue(int hue) { mBaseHue = hue; update(); }

void ColorRampBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int segW = w / mClassCount;

    for (int i = 0; i < mClassCount; ++i)
    {
        int hue = (mBaseHue + i * 360 / mClassCount) % 360;
        QColor c = QColor::fromHsv(hue, 200, 240);
        QRect seg(i * segW, 1, segW, h - 2);
        p.fillRect(seg, c);
    }

    p.setPen(QColor(160, 160, 160));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);
}

// ═══════════════════════════════════════════════════════════════════════
// VectorStyleDialog
// ═══════════════════════════════════════════════════════════════════════
VectorStyleDialog::VectorStyleDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("矢量图层样式设置"));
    setMinimumWidth(420);
    setupUI();
}

void VectorStyleDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // ── Group: 渲染模式 ──
    auto* modeGroup = new QGroupBox(QStringLiteral("渲染模式"), this);
    auto* modeLayout = new QFormLayout(modeGroup);

    mStyleTypeCombo = new QComboBox(this);
    mStyleTypeCombo->addItem(QStringLiteral("单一符号 — 所有要素同色"));
    mStyleTypeCombo->addItem(QStringLiteral("分类着色 — 按字段值分色"));
    mStyleTypeCombo->addItem(QStringLiteral("渐变着色 — 按数值字段渐变"));

    mFieldCombo = new QComboBox(this);
    mFieldCombo->setMinimumWidth(160);

    modeLayout->addRow(QStringLiteral("渲染模式:"), mStyleTypeCombo);
    modeLayout->addRow(QStringLiteral("分类字段:"), mFieldCombo);
    mainLayout->addWidget(modeGroup);

    // ── Stacked: per-style parameters ──
    mStack = new QStackedWidget(this);

    // ── Page 0: Single Symbol ──
    auto* singlePage = new QWidget(this);
    auto* singleLayout = new QHBoxLayout(singlePage);
    singleLayout->setSpacing(20);

    // Fill color
    auto* fillCol = new QVBoxLayout();
    auto* fillTitle = new QLabel(QStringLiteral("填充颜色"), this);
    fillTitle->setAlignment(Qt::AlignCenter);
    mFillSwatch = new ColorSwatch(this);
    mFillSwatch->setColor(mConfig.fillColor);
    mFillSwatch->setFixedSize(48, 32);
    mFillLabel = new QLabel(mConfig.fillColor.name().toUpper(), this);
    mFillLabel->setAlignment(Qt::AlignCenter);
    QFont mono(QStringLiteral("Consolas"));
    mono.setPointSize(9);
    mFillLabel->setFont(mono);
    fillCol->addWidget(fillTitle, 0, Qt::AlignCenter);
    fillCol->addWidget(mFillSwatch, 0, Qt::AlignCenter);
    fillCol->addWidget(mFillLabel, 0, Qt::AlignCenter);
    connect(mFillSwatch, &ColorSwatch::clicked, this, &VectorStyleDialog::onFillClicked);
    singleLayout->addLayout(fillCol);

    // Stroke color
    auto* strokeCol = new QVBoxLayout();
    auto* strokeTitle = new QLabel(QStringLiteral("边线颜色"), this);
    strokeTitle->setAlignment(Qt::AlignCenter);
    mStrokeSwatch = new ColorSwatch(this);
    mStrokeSwatch->setColor(mConfig.strokeColor);
    mStrokeSwatch->setFixedSize(48, 32);
    mStrokeLabel = new QLabel(mConfig.strokeColor.name().toUpper(), this);
    mStrokeLabel->setAlignment(Qt::AlignCenter);
    mStrokeLabel->setFont(mono);
    strokeCol->addWidget(strokeTitle, 0, Qt::AlignCenter);
    strokeCol->addWidget(mStrokeSwatch, 0, Qt::AlignCenter);
    strokeCol->addWidget(mStrokeLabel, 0, Qt::AlignCenter);
    connect(mStrokeSwatch, &ColorSwatch::clicked, this, &VectorStyleDialog::onStrokeClicked);
    singleLayout->addLayout(strokeCol);

    // Stroke width
    auto* widthCol = new QVBoxLayout();
    auto* widthTitle = new QLabel(QStringLiteral("线宽"), this);
    widthTitle->setAlignment(Qt::AlignCenter);
    mStrokeWidthSpin = new QDoubleSpinBox(this);
    mStrokeWidthSpin->setRange(0.1, 10.0);
    mStrokeWidthSpin->setValue(mConfig.strokeWidth);
    mStrokeWidthSpin->setSingleStep(0.5);
    mStrokeWidthSpin->setSuffix(QStringLiteral(" px"));
    mStrokeWidthSpin->setFixedWidth(80);
    widthCol->addWidget(widthTitle, 0, Qt::AlignCenter);
    widthCol->addWidget(mStrokeWidthSpin, 0, Qt::AlignCenter);
    widthCol->addStretch();
    singleLayout->addLayout(widthCol);

    singleLayout->addStretch();
    mStack->addWidget(singlePage);

    // ── Page 1: Categorized / Graduated ──
    mClassifyPage = new QWidget(this);
    auto* classifyLayout = new QVBoxLayout(mClassifyPage);
    classifyLayout->setSpacing(8);

    // Class count + method row
    auto* paramRow = new QHBoxLayout();
    mClassCountSpin = new QSpinBox(this);
    mClassCountSpin->setRange(2, 20);
    mClassCountSpin->setValue(5);
    mClassCountSpin->setPrefix(QStringLiteral("分类数: "));
    paramRow->addWidget(mClassCountSpin);

    mClassMethodCombo = new QComboBox(this);
    mClassMethodCombo->addItem(QStringLiteral("Jenks 自然断点"));
    mClassMethodCombo->addItem(QStringLiteral("等间隔 (Equal Interval)"));
    mClassMethodCombo->addItem(QStringLiteral("分位数 (Quantile)"));
    mClassMethodCombo->addItem(QStringLiteral("标准差 (StdDev)"));
    paramRow->addWidget(mClassMethodCombo);
    paramRow->addStretch();
    classifyLayout->addLayout(paramRow);

    // Color ramp preview
    auto* rampLabel = new QLabel(QStringLiteral("色带预览:"), this);
    classifyLayout->addWidget(rampLabel);
    mRampBar = new ColorRampBar(this);
    classifyLayout->addWidget(mRampBar);

    classifyLayout->addStretch();
    mStack->addWidget(mClassifyPage);

    mainLayout->addWidget(mStack);

    // ── Buttons ──
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("应用"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        mConfig.styleType = static_cast<VectorStyleType>(mStyleTypeCombo->currentIndex());
        mConfig.classifyField = mFieldCombo->currentText();
        mConfig.classCount = mClassCountSpin->value();
        mConfig.strokeWidth = mStrokeWidthSpin->value();
        mConfig.fillColor = mFillSwatch->color();
        mConfig.strokeColor = mStrokeSwatch->color();
        const QString& m = mClassMethodCombo->currentText();
        if (m.contains(QStringLiteral("Jenks"))) mConfig.classificationMethod = QStringLiteral("Jenks");
        else if (m.contains(QString::fromUtf8("\xe7\xad\x89\xe9\x97\xb4\xe9\x9a\x94"))) mConfig.classificationMethod = QStringLiteral("EqualInterval");
        else if (m.contains(QString::fromUtf8("\xe5\x88\x86\xe4\xbd\x8d\xe6\x95\xb0"))) mConfig.classificationMethod = QStringLiteral("Quantile");
        else if (m.contains(QString::fromUtf8("\xe6\xa0\x87\xe5\x87\x86\xe5\xb7\xae"))) mConfig.classificationMethod = QStringLiteral("StdDev");
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    // Signals
    connect(mStyleTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VectorStyleDialog::onStyleTypeChanged);
    connect(mClassCountSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &VectorStyleDialog::onClassParamChanged);
}

void VectorStyleDialog::onStyleTypeChanged(int index)
{
    auto type = static_cast<VectorStyleType>(index);
    mStack->setCurrentIndex(type == VectorStyleType::SingleSymbol ? 0 : 1);
    onClassParamChanged();
}

void VectorStyleDialog::onFillClicked()
{
    QColor c = askColor(mConfig.fillColor, QStringLiteral("填充颜色"));
    if (c.isValid()) {
        mConfig.fillColor = c;
        mFillSwatch->setColor(c);
        mFillLabel->setText(c.name().toUpper());
    }
}

void VectorStyleDialog::onStrokeClicked()
{
    QColor c = askColor(mConfig.strokeColor, QStringLiteral("边线颜色"));
    if (c.isValid()) {
        mConfig.strokeColor = c;
        mStrokeSwatch->setColor(c);
        mStrokeLabel->setText(c.name().toUpper());
    }
}

void VectorStyleDialog::onClassParamChanged()
{
    mRampBar->setClassCount(mClassCountSpin->value());
}

QColor VectorStyleDialog::askColor(const QColor& initial, const QString& title)
{
    return QColorDialog::getColor(initial, this, title);
}

void VectorStyleDialog::setConfig(const VectorStyleConfig& cfg)
{
    mConfig = cfg;
    mStyleTypeCombo->setCurrentIndex(static_cast<int>(cfg.styleType));
    mFieldCombo->setCurrentText(cfg.classifyField);
    mClassCountSpin->setValue(cfg.classCount);
    mStrokeWidthSpin->setValue(cfg.strokeWidth);

    int methIdx = 0;
    if (cfg.classificationMethod == QStringLiteral("EqualInterval")) methIdx = 1;
    else if (cfg.classificationMethod == QStringLiteral("Quantile")) methIdx = 2;
    else if (cfg.classificationMethod == QStringLiteral("StdDev")) methIdx = 3;
    mClassMethodCombo->setCurrentIndex(methIdx);

    mFillSwatch->setColor(cfg.fillColor);
    mFillLabel->setText(cfg.fillColor.name().toUpper());
    mStrokeSwatch->setColor(cfg.strokeColor);
    mStrokeLabel->setText(cfg.strokeColor.name().toUpper());

    onStyleTypeChanged(static_cast<int>(cfg.styleType));
}

void VectorStyleDialog::setFieldNames(const QStringList& names)
{
    mFieldCombo->clear();
    mFieldCombo->addItems(names);
}

VectorStyleConfig VectorStyleDialog::config() const
{
    return mConfig;
}
