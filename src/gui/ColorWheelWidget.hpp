#pragma once

#include <QWidget>
#include <QColor>

namespace Kaburagi {

/**
 * @brief PaintTool SAI風 HSVカラーホイールウィジェット
 * 外周：色相(Hue)サークル (9時:赤, 12時:黄, 3時:シアン, 6時:青)
 * 内側：彩度(Saturation)・明度(Value)の矩形領域
 * マーカー：SAI特有のオレンジ色ドーナツ型リング
 */
class ColorWheelWidget : public QWidget {
    Q_OBJECT
public:
    explicit ColorWheelWidget(QWidget* parent = nullptr);
    ~ColorWheelWidget() override = default;

    QColor currentColor() const;
    void setColor(const QColor& color);

    // SAI配置の角度 (0°=9時, 時計回り) と色相 (0°〜360°) の相互変換
    static double wheelAngleToHue(double angle);
    static double hueToWheelAngle(double hue);

signals:
    void colorChanged(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void tabletEvent(QTabletEvent* event) override;
    QSize sizeHint() const override { return QSize(160, 160); }
    QSize minimumSizeHint() const override { return QSize(120, 120); }

private:
    void handleMousePos(const QPointF& pos);
    QRectF getInnerSquare() const;
    void drawSaiRingMarker(QPainter& painter, const QPointF& pos);
    void invalidateCache();

    float m_hue = 210.0f;      // デフォルト: SAI風の青空色
    float m_saturation = 0.8f; // 0.0 ~ 1.0
    float m_value = 0.85f;     // 0.0 ~ 1.0

    enum class DragMode { None, HueRing, SVBox };
    DragMode m_dragMode = DragMode::None;

    QPixmap m_cachedRingPixmap;
    QSize m_cachedRingWidgetSize;
    QImage m_cachedSvImage;
    float m_cachedSvHue = -1.0f;
    QSize m_cachedSvSize;
};

} // namespace Kaburagi
