#pragma once

#include <QWidget>
#include <QColor>
#include <QScrollBar>
#include <vector>

namespace Kaburagi {

/**
 * @brief PaintTool SAI風カラーパレット（横12 x 縦12セル、スクロール可能）
 */
class ColorPaletteWidget : public QWidget {
    Q_OBJECT
public:
    explicit ColorPaletteWidget(QWidget* parent = nullptr);
    ~ColorPaletteWidget() override = default;

    static constexpr int COLS = 12;
    static constexpr int VISIBLE_ROWS = 4;

    int totalRows() const { return m_rows; }
    int totalCells() const { return static_cast<int>(m_colors.size()); }

    QColor colorAt(int index) const;
    void setColorAt(int index, const QColor& color);
    void addColor(const QColor& color);

    void setCurrentColor(const QColor& color) { m_currentColor = color; }
    QColor currentColor() const { return m_currentColor; }

    void clearPalette();
    bool importAco(const QString& filePath, bool append = false);
    bool exportAco(const QString& filePath) const;

    void saveSettings();
    void loadSettings();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void colorSelected(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void showContextMenu(const QPoint& pos, int col, int row);
    void initDefaultColors();
    void updateScrollRange();
    int cellFromPos(const QPoint& pos, int& outCol, int& outRow) const;

    int m_rows = 16;                   // 総行数（デフォルト16行、12行表示+スクロール余地）
    int m_cellSize = 13;               // 1マスのサイズ (px)
    int m_hoverIndex = -1;             // ホバー中セルのインデックス
    QColor m_currentColor = Qt::black; // 登録用現在の描画色

    std::vector<QColor> m_colors;
    QScrollBar* m_scrollBar = nullptr;
    QPixmap m_checkerPixmap;           // 未登録セルの市松模様
};

} // namespace Kaburagi
