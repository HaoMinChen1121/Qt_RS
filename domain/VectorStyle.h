#ifndef VECTORSTYLE_H
#define VECTORSTYLE_H

#include <QColor>
#include <QString>
#include <QMetaType>

enum class VectorStyleType
{
    SingleSymbol,
    Categorized,
    Graduated
};

struct VectorStyleConfig
{
    VectorStyleType styleType = VectorStyleType::SingleSymbol;

    // ── 分类/渐变共用 ──
    QString classifyField;                       // 分类字段名
    int     classCount           = 5;            // 分类/分级数
    QString classificationMethod = QStringLiteral("Jenks");  // Jenks / EqualInterval / Quantile / StdDev
    QString colorRampName        = QStringLiteral("Spectral"); // Spectral / Viridis / Blues / Reds / Pastel

    // ── 单一符号 ──
    QColor fillColor   = QColor(56, 168, 0, 128);
    QColor strokeColor = QColor(56, 168, 0);
    double strokeWidth = 1.0;
    double markerSize  = 4.0;
};

Q_DECLARE_METATYPE(VectorStyleConfig)

#endif // VECTORSTYLE_H
