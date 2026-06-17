#include "ExportDialog.h"
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>

ExportDialog::ExportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("导出选项"));
    setMinimumWidth(320);

    auto* mainLayout = new QVBoxLayout(this);

    auto* form = new QFormLayout();

    mCompressionCombo = new QComboBox(this);
    mCompressionCombo->addItem(tr("无压缩"), QStringLiteral("NONE"));
    mCompressionCombo->addItem(tr("LZW (无损，推荐)"), QStringLiteral("LZW"));
    mCompressionCombo->addItem(tr("DEFLATE (无损，体积最小)"), QStringLiteral("DEFLATE"));
    mCompressionCombo->addItem(tr("PACKBITS (快速)"), QStringLiteral("PACKBITS"));
    mCompressionCombo->setCurrentIndex(1);
    form->addRow(tr("压缩方式:"), mCompressionCombo);

    mPredictorCheck = new QCheckBox(tr("启用水平预测器 (PREDICTOR=2)"), this);
    mPredictorCheck->setChecked(true);
    mPredictorCheck->setToolTip(tr("对 16-bit 整数数据压缩效果显著提升"));
    form->addRow(mPredictorCheck);

    mPyramidCheck = new QCheckBox(tr("构建金字塔概视图 (加速渲染)"), this);
    mPyramidCheck->setChecked(true);
    mPyramidCheck->setToolTip(tr("取消可节省 20-30 秒导出时间，但后续渲染会变慢"));
    form->addRow(mPyramidCheck);

    mEstimatedSizeLabel = new QLabel(this);
    mEstimatedSizeLabel->setStyleSheet("color: gray;");
    form->addRow(tr("预估:"), mEstimatedSizeLabel);

    mainLayout->addLayout(form);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    connect(mCompressionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (idx == 0)
            mEstimatedSizeLabel->setText(tr("不压缩，文件很大"));
        else if (idx == 1)
            mEstimatedSizeLabel->setText(tr("约原始大小的 40-60%"));
        else if (idx == 2)
            mEstimatedSizeLabel->setText(tr("约原始大小的 35-55%"));
        else
            mEstimatedSizeLabel->setText(tr("约原始大小的 60-80%"));
    });
}

ExportOptions ExportDialog::options() const
{
    ExportOptions opts;
    opts.compression = mCompressionCombo->currentData().toString();
    opts.usePredictor = mPredictorCheck->isChecked();
    opts.buildPyramids = mPyramidCheck->isChecked();
    return opts;
}
