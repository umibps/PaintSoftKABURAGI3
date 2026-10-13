#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QTimer>

namespace Kaburagi {

/**
 * @brief GIMP風 SpinScale カスタムウィジェット
 * ・マウスのドラッグで値変更
 * ・キーボードからの数値直接入力 (クリックまたはダブルクリック)
 * ・ウィジェット右端の上下スピンボタン
 * ・ウィジェット内部の左端にラベル表示
 * ・背景に進捗バー表示
 */
class SpinScale : public QWidget {
    Q_OBJECT
public:
    explicit SpinScale(const QString& label, double min, double max, double step, int decimals = 2, const QString& suffix = "", QWidget* parent = nullptr);
    ~SpinScale() override = default;

    double value() const { return m_value; }
    void setValue(double val);

    void setRange(double min, double max);
    void setLabel(const QString& label);
    void setDecimals(int decimals);
    void setStep(double step);
    void setSuffix(const QString& suffix);

signals:
    void valueChanged(double value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    QSize sizeHint() const override { return QSize(160, 24); }
    QSize minimumSizeHint() const override { return QSize(100, 22); }

private slots:
    void onLineEditFinished();
    void onSpinButtonTimeout();

private:
    QRect arrowArea() const;
    QRect upArrowRect() const;
    QRect downArrowRect() const;
    void updateValueFromMousePos(int x);
    void startTextEditing();

    QString m_label;
    double m_min = 0.0;
    double m_max = 100.0;
    double m_step = 1.0;
    int m_decimals = 2;
    QString m_suffix;
    double m_value = 0.0;

    bool m_isDragging = false;
    bool m_isArrowPressed = false;
    bool m_isUpArrow = false;

    QLineEdit* m_lineEdit = nullptr;
    QTimer* m_spinTimer = nullptr;
};

} // namespace Kaburagi
