#pragma once

#include <QWidget>
#include <array>
#include "AdjustmentLayer.hpp"

namespace Kaburagi {

class LevelsWidget : public QWidget {
    Q_OBJECT
public:
    explicit LevelsWidget(QWidget* parent = nullptr);
    ~LevelsWidget() override = default;

    void setLevelParams(AdjustmentChannel ch, const LevelParams& params);
    const LevelParams& levelParams(AdjustmentChannel ch) const;

    void setCurrentChannel(AdjustmentChannel ch);
    AdjustmentChannel currentChannel() const { return m_channel; }

    void setHistogram(const std::array<int, 256>& hist);

    void resetCurrentLevels();
    void resetAllLevels();

signals:
    void levelsChanged(AdjustmentChannel ch, const LevelParams& params);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class MarkerType {
        None,
        InShadow,
        InGamma,
        InHighlight,
        OutShadow,
        OutHighlight
    };

    QRectF histAreaRect() const;
    QRectF inBarRect() const;
    QRectF outBarRect() const;

    qreal gammaToPos(float gamma, qreal xShadow, qreal xHighlight) const;
    float posToGamma(qreal xPos, qreal xShadow, qreal xHighlight) const;

    MarkerType hitTestMarker(const QPointF& pos) const;

    AdjustmentChannel m_channel = AdjustmentChannel::RGB;
    std::array<LevelParams, 4> m_params;

    std::array<int, 256> m_histogram{};
    int m_maxHistCount = 1;

    MarkerType m_draggingMarker = MarkerType::None;
    MarkerType m_hoverMarker = MarkerType::None;
};

} // namespace Kaburagi
