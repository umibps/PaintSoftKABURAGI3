#include "LevelsWidget.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QLinearGradient>
#include <algorithm>
#include <cmath>

namespace Kaburagi {

LevelsWidget::LevelsWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(180, 160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);

    resetAllLevels();
}

void LevelsWidget::resetAllLevels() {
    for (size_t i = 0; i < 4; ++i) {
        m_params[i] = LevelParams();
    }
    m_draggingMarker = MarkerType::None;
    m_hoverMarker = MarkerType::None;
    update();
}

void LevelsWidget::resetCurrentLevels() {
    size_t idx = static_cast<size_t>(m_channel);
    m_params[idx] = LevelParams();
    m_draggingMarker = MarkerType::None;
    m_hoverMarker = MarkerType::None;
    update();
    emit levelsChanged(m_channel, m_params[idx]);
}

void LevelsWidget::setLevelParams(AdjustmentChannel ch, const LevelParams& params) {
    m_params[static_cast<size_t>(ch)] = params;
    update();
}

const LevelParams& LevelsWidget::levelParams(AdjustmentChannel ch) const {
    return m_params[static_cast<size_t>(ch)];
}

void LevelsWidget::setCurrentChannel(AdjustmentChannel ch) {
    if (m_channel == ch) return;
    m_channel = ch;
    m_draggingMarker = MarkerType::None;
    m_hoverMarker = MarkerType::None;
    update();
}

void LevelsWidget::setHistogram(const std::array<int, 256>& hist) {
    m_histogram = hist;
    m_maxHistCount = 1;
    for (int count : m_histogram) {
        if (count > m_maxHistCount) m_maxHistCount = count;
    }
    update();
}

QRectF LevelsWidget::histAreaRect() const {
    qreal margin = 10.0;
    qreal w = width() - margin * 2.0;
    qreal h = 75.0;
    return QRectF(margin, 8.0, w, h);
}

QRectF LevelsWidget::inBarRect() const {
    QRectF ha = histAreaRect();
    return QRectF(ha.left(), ha.bottom() + 4.0, ha.width(), 8.0);
}

QRectF LevelsWidget::outBarRect() const {
    QRectF inBar = inBarRect();
    return QRectF(inBar.left(), inBar.bottom() + 28.0, inBar.width(), 8.0);
}

qreal LevelsWidget::gammaToPos(float gamma, qreal xShadow, qreal xHighlight) const {
    // ガンマ 1.0 が中間位置、0.1 がシャドウ寄り、10.0 がハイライト寄り
    // 対数マッピング: t = 0.5 + 0.5 * log10(gamma)
    float logG = std::log10(std::clamp(gamma, 0.1f, 10.0f));
    qreal t = std::clamp(static_cast<qreal>(0.5f + 0.5f * logG), 0.0, 1.0);
    return xShadow + t * (xHighlight - xShadow);
}

float LevelsWidget::posToGamma(qreal xPos, qreal xShadow, qreal xHighlight) const {
    qreal span = std::max(1.0, xHighlight - xShadow);
    qreal t = std::clamp((xPos - xShadow) / span, 0.02, 0.98);
    float logG = static_cast<float>((t - 0.5) * 2.0);
    return std::clamp(std::pow(10.0f, logG), 0.1f, 9.99f);
}

LevelsWidget::MarkerType LevelsWidget::hitTestMarker(const QPointF& pos) const {
    const auto& p = m_params[static_cast<size_t>(m_channel)];
    QRectF inBar = inBarRect();
    QRectF outBar = outBarRect();

    qreal xInShadow = inBar.left() + (p.inShadow / 255.0) * inBar.width();
    qreal xInHighlight = inBar.left() + (p.inHighlight / 255.0) * inBar.width();
    qreal xInGamma = gammaToPos(p.inGamma, xInShadow, xInHighlight);

    qreal xOutShadow = outBar.left() + (p.outShadow / 255.0) * outBar.width();
    qreal xOutHighlight = outBar.left() + (p.outHighlight / 255.0) * outBar.width();

    const qreal hitDist = 7.0;

    // 入力マーカー判定 (inBar の下)
    if (std::abs(pos.y() - (inBar.bottom() + 6.0)) <= 10.0) {
        if (std::abs(pos.x() - xInGamma) <= hitDist) return MarkerType::InGamma;
        if (std::abs(pos.x() - xInShadow) <= hitDist) return MarkerType::InShadow;
        if (std::abs(pos.x() - xInHighlight) <= hitDist) return MarkerType::InHighlight;
    }

    // 出力マーカー判定 (outBar の下)
    if (std::abs(pos.y() - (outBar.bottom() + 6.0)) <= 10.0) {
        if (std::abs(pos.x() - xOutShadow) <= hitDist) return MarkerType::OutShadow;
        if (std::abs(pos.x() - xOutHighlight) <= hitDist) return MarkerType::OutHighlight;
    }

    return MarkerType::None;
}

void LevelsWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRectF histArea = histAreaRect();
    QRectF inBar = inBarRect();
    QRectF outBar = outBarRect();

    // 1. ヒストグラム背景
    painter.fillRect(histArea, QColor(245, 245, 245));

    // 2. ヒストグラムバー描画
    if (m_maxHistCount > 0) {
        QColor histColor;
        switch (m_channel) {
        case AdjustmentChannel::Red: histColor = QColor(255, 100, 100, 90); break;
        case AdjustmentChannel::Green: histColor = QColor(100, 200, 100, 90); break;
        case AdjustmentChannel::Blue: histColor = QColor(100, 150, 255, 90); break;
        default: histColor = QColor(140, 140, 140, 80); break;
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(histColor);

        for (int i = 0; i < 256; ++i) {
            qreal barH = (static_cast<qreal>(m_histogram[i]) / m_maxHistCount) * histArea.height();
            if (barH > 0.5) {
                qreal bx = histArea.left() + (i / 255.0) * histArea.width();
                qreal bw = histArea.width() / 256.0 + 0.5;
                painter.drawRect(QRectF(bx, histArea.bottom() - barH, bw, barH));
            }
        }
    }

    painter.setPen(QPen(QColor(140, 140, 140), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(histArea);

    // 3. 入力レベルバー (黒〜白グラデーション)
    QLinearGradient inGrad(inBar.topLeft(), inBar.topRight());
    inGrad.setColorAt(0.0, Qt::black);
    inGrad.setColorAt(1.0, Qt::white);
    painter.fillRect(inBar, inGrad);
    painter.setPen(QPen(QColor(120, 120, 120), 1.0));
    painter.drawRect(inBar);

    // 4. 出力レベルバー (黒〜白グラデーション)
    QLinearGradient outGrad(outBar.topLeft(), outBar.topRight());
    outGrad.setColorAt(0.0, Qt::black);
    outGrad.setColorAt(1.0, Qt::white);
    painter.fillRect(outBar, outGrad);
    painter.setPen(QPen(QColor(120, 120, 120), 1.0));
    painter.drawRect(outBar);

    // 三角形マーカー描画関数
    auto drawTriangleMarker = [&](qreal cx, qreal topY, const QColor& fillColor, bool isHover) {
        QPolygonF poly;
        poly << QPointF(cx, topY)
             << QPointF(cx - 5.0, topY + 8.0)
             << QPointF(cx + 5.0, topY + 8.0);
        painter.setPen(QPen(isHover ? QColor(255, 120, 0) : QColor(40, 40, 40), 1.2));
        painter.setBrush(fillColor);
        painter.drawPolygon(poly);
    };

    const auto& p = m_params[static_cast<size_t>(m_channel)];

    // 入力マーカー
    qreal xInShadow = inBar.left() + (p.inShadow / 255.0) * inBar.width();
    qreal xInHighlight = inBar.left() + (p.inHighlight / 255.0) * inBar.width();
    qreal xInGamma = gammaToPos(p.inGamma, xInShadow, xInHighlight);

    drawTriangleMarker(xInShadow, inBar.bottom() + 1.0, Qt::black, m_hoverMarker == MarkerType::InShadow || m_draggingMarker == MarkerType::InShadow);
    drawTriangleMarker(xInGamma, inBar.bottom() + 1.0, QColor(140, 140, 140), m_hoverMarker == MarkerType::InGamma || m_draggingMarker == MarkerType::InGamma);
    drawTriangleMarker(xInHighlight, inBar.bottom() + 1.0, Qt::white, m_hoverMarker == MarkerType::InHighlight || m_draggingMarker == MarkerType::InHighlight);

    // 出力マーカー
    qreal xOutShadow = outBar.left() + (p.outShadow / 255.0) * outBar.width();
    qreal xOutHighlight = outBar.left() + (p.outHighlight / 255.0) * outBar.width();

    drawTriangleMarker(xOutShadow, outBar.bottom() + 1.0, Qt::black, m_hoverMarker == MarkerType::OutShadow || m_draggingMarker == MarkerType::OutShadow);
    drawTriangleMarker(xOutHighlight, outBar.bottom() + 1.0, Qt::white, m_hoverMarker == MarkerType::OutHighlight || m_draggingMarker == MarkerType::OutHighlight);

    // テキストラベル ("入力レベル", "出力レベル")
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);
    painter.setPen(QColor(80, 80, 80));
    painter.drawText(QRectF(outBar.left(), inBar.bottom() + 12.0, outBar.width(), 14.0), Qt::AlignLeft | Qt::AlignVCenter, "出力レベル");
}

void LevelsWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_draggingMarker = hitTestMarker(event->position());
        update();
    }
}

void LevelsWidget::mouseMoveEvent(QMouseEvent* event) {
    auto& p = m_params[static_cast<size_t>(m_channel)];
    QRectF inBar = inBarRect();
    QRectF outBar = outBarRect();

    if (m_draggingMarker != MarkerType::None && (event->buttons() & Qt::LeftButton)) {
        qreal xInShadow = inBar.left() + (p.inShadow / 255.0) * inBar.width();
        qreal xInHighlight = inBar.left() + (p.inHighlight / 255.0) * inBar.width();

        switch (m_draggingMarker) {
        case MarkerType::InShadow: {
            int val = std::clamp(static_cast<int>(std::round((event->position().x() - inBar.left()) / inBar.width() * 255.0)), 0, p.inHighlight - 1);
            p.inShadow = val;
            break;
        }
        case MarkerType::InGamma: {
            p.inGamma = posToGamma(event->position().x(), xInShadow, xInHighlight);
            break;
        }
        case MarkerType::InHighlight: {
            int val = std::clamp(static_cast<int>(std::round((event->position().x() - inBar.left()) / inBar.width() * 255.0)), p.inShadow + 1, 255);
            p.inHighlight = val;
            break;
        }
        case MarkerType::OutShadow: {
            int val = std::clamp(static_cast<int>(std::round((event->position().x() - outBar.left()) / outBar.width() * 255.0)), 0, p.outHighlight);
            p.outShadow = val;
            break;
        }
        case MarkerType::OutHighlight: {
            int val = std::clamp(static_cast<int>(std::round((event->position().x() - outBar.left()) / outBar.width() * 255.0)), p.outShadow, 255);
            p.outHighlight = val;
            break;
        }
        default:
            break;
        }

        update();
        emit levelsChanged(m_channel, p);
    } else {
        MarkerType hover = hitTestMarker(event->position());
        if (hover != m_hoverMarker) {
            m_hoverMarker = hover;
            update();
        }
    }
}

void LevelsWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_draggingMarker = MarkerType::None;
        update();
    }
}

} // namespace Kaburagi
