#ifndef VECTORSTYLEDIALOG_H
#define VECTORSTYLEDIALOG_H

#include <QDialog>
#include "domain/VectorStyle.h"

class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QStackedWidget;
class QLabel;
class QSlider;

class ColorSwatch : public QWidget
{
    Q_OBJECT
public:
    explicit ColorSwatch(QWidget* parent = nullptr);
    void setColor(const QColor& c);
    QColor color() const { return mColor; }
signals:
    void clicked();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
private:
    QColor mColor;
};

class ColorRampBar : public QWidget
{
    Q_OBJECT
public:
    explicit ColorRampBar(QWidget* parent = nullptr);
    void setClassCount(int count);
    void setBaseHue(int hue);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    int mClassCount = 5;
    int mBaseHue = 0;
};

class VectorStyleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VectorStyleDialog(QWidget* parent = nullptr);

    void setConfig(const VectorStyleConfig& cfg);
    void setFieldNames(const QStringList& names);
    VectorStyleConfig config() const;

private slots:
    void onStyleTypeChanged(int index);
    void onFillClicked();
    void onStrokeClicked();
    void onClassParamChanged();

private:
    void setupUI();
    QColor askColor(const QColor& initial, const QString& title);

    VectorStyleConfig mConfig;

    QComboBox*     mStyleTypeCombo;
    QComboBox*     mFieldCombo;
    QSpinBox*      mClassCountSpin;
    QComboBox*     mClassMethodCombo;
    QDoubleSpinBox* mStrokeWidthSpin;
    QStackedWidget* mStack;

    // Page 0: Single Symbol
    ColorSwatch*   mFillSwatch;
    ColorSwatch*   mStrokeSwatch;
    QLabel*        mFillLabel;
    QLabel*        mStrokeLabel;

    // Page 1: Categorized / Graduated
    QWidget*       mClassifyPage;
    ColorRampBar*  mRampBar;
};

#endif // VECTORSTYLEDIALOG_H
