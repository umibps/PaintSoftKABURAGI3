#pragma once

#include <QWidget>
#include <QColor>

namespace Kaburagi {

/**
 * @brief PaintTool SAI風 描画色・背景色の重なり表示＆入れ替えウィジェット
 */
class ColorBoxWidget : public QWidget {
    Q_OBJECT
public:
    explicit ColorBoxWidget(QWidget* parent = nullptr);
    ~ColorBoxWidget() override = default;

    QColor primaryColor() const { return m_primaryColor; }
    QColor secondaryColor() const { return m_secondaryColor; }
    QColor activeColor() const { return m_isPrimaryActive ? m_primaryColor : m_secondaryColor; }
    bool isPrimaryActive() const { return m_isPrimaryActive; }

    void setPrimaryColor(const QColor& color);
    void setSecondaryColor(const QColor& color);
    void setCurrentActiveColor(const QColor& color);

public slots:
    void swapColors();
    void resetToDefaultColors(); // 黒と白にリセット

signals:
    void primaryColorChanged(const QColor& color);
    void secondaryColorChanged(const QColor& color);
    void activeColorChanged(const QColor& color);
    void activeTargetChanged(bool isPrimary);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return QSize(60, 52); }
    QSize minimumSizeHint() const override { return QSize(54, 48); }

private:
    QRect primaryRect() const;
    QRect secondaryRect() const;
    QRect swapButtonRect() const;

    QColor m_primaryColor = QColor(0, 0, 0);       // 描画色 (初期黒)
    QColor m_secondaryColor = QColor(255, 255, 255); // 背景色 (初期白)
    bool m_isPrimaryActive = true;
};

} // namespace Kaburagi
