#pragma once

#include <QWidget>
#include <vector>
#include <array>
#include <QPointF>
#include "AdjustmentLayer.hpp"

namespace Kaburagi {

class ToneCurveWidget : public QWidget {
    Q_OBJECT
public:
    explicit ToneCurveWidget(QWidget* parent = nullptr);
    ~ToneCurveWidget() override = default;

    void setCurvePoints(AdjustmentChannel ch, const std::vector<QPointF>& points);
    const std::vector<QPointF>& curvePoints(AdjustmentChannel ch) const;

    void setCurrentChannel(AdjustmentChannel ch);
    AdjustmentChannel currentChannel() const { return m_channel; }

    void setInterpolationMode(CurveInterpolationMode mode);
    CurveInterpolationMode interpolationMode() const { return m_interpMode; }

    void setHistogram(const std::array<int, 256>& hist);

    void resetCurrentCurve();
    void resetAllCurves();

signals:
    void curveChanged(AdjustmentChannel ch, const std::vector<QPointF>& points);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QRectF curveAreaRect() const;
    QPointF curveToWidget(const QPointF& pt) const;
    QPointF widgetToCurve(const QPointF& pos) const;
    int findHoverPoint(const QPointF& pos) const;

    AdjustmentChannel m_channel = AdjustmentChannel::RGB;
    CurveInterpolationMode m_interpMode = CurveInterpolationMode::Spline;

    // 各チャンネル (RGB, R, G, B) の制御点 (0~255)
    std::array<std::vector<QPointF>, 4> m_points;

    // 現在表示中のヒストグラム (0~255)
    std::array<int, 256> m_histogram{};
    int m_maxHistCount = 1;

    int m_selectedPointIdx = -1;
    int m_hoverPointIdx = -1;
    bool m_isDragging = false;
};

} // namespace Kaburagi
