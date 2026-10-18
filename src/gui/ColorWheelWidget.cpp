#include "ColorWheelWidget.hpp"
#include "DisplayFilterManager.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QPainterPath>
#include <QConicalGradient>
#include <cmath>
#include <vector>
#include <algorithm>

namespace Kaburagi {

struct AngleHuePair {
    double angle; // 0 ~ 360 (0° = 9時/左, 時計回り)
    double hue;   // 0 ~ 360 (HSV 色相)
};

// SAIの色相環マッピングテーブル:
// 9時(0°): 赤 (0°), 10時半(45°): 橙 (30°), 12時(90°): 黄 (60°),
// 1時半(135°): 緑 (120°), 3時(180°): シアン (180°), 4時半(225°): 空色 (210°),
// 6時(270°): 青 (240°), 7時半(315°): マゼンタ (300°), 9時(360°): 赤 (360°)
static const std::vector<AngleHuePair> s_map = {
    {   0.0,   0.0 }, // 9時 (赤)
    {  45.0,  30.0 }, // 10時半 (橙)
    {  90.0,  60.0 }, // 12時 (黄)
    { 135.0, 120.0 }, // 1時半 (緑)
    { 180.0, 180.0 }, // 3時 (シアン)
    { 225.0, 210.0 }, // 4時半 (空色)
    { 270.0, 240.0 }, // 6時 (青)
    { 315.0, 300.0 }, // 7時半 (マゼンタ)
    { 360.0, 360.0 }  // 9時 (赤)
};

double ColorWheelWidget::wheelAngleToHue(double angle) {
    double a = std::fmod(angle, 360.0);
    if (a < 0) a += 360.0;

    for (size_t i = 0; i < s_map.size() - 1; ++i) {
        if (a >= s_map[i].angle && a <= s_map[i + 1].angle) {
            double spanA = s_map[i + 1].angle - s_map[i].angle;
            double t = (spanA > 0.0) ? (a - s_map[i].angle) / spanA : 0.0;
            return s_map[i].hue + t * (s_map[i + 1].hue - s_map[i].hue);
        }
    }
    return 0.0;
}

double ColorWheelWidget::hueToWheelAngle(double hue) {
    double h = std::fmod(hue, 360.0);
    if (h < 0) h += 360.0;

    for (size_t i = 0; i < s_map.size() - 1; ++i) {
        if (h >= s_map[i].hue && h <= s_map[i + 1].hue) {
            double spanH = s_map[i + 1].hue - s_map[i].hue;
            double t = (spanH > 0.0) ? (h - s_map[i].hue) / spanH : 0.0;
            return s_map[i].angle + t * (s_map[i + 1].angle - s_map[i].angle);
        }
    }
    return 0.0;
}

ColorWheelWidget::ColorWheelWidget(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    connect(&DisplayFilterManager::instance(), &DisplayFilterManager::filterChanged, this, [this]() {
        invalidateCache();
        update();
    });
}

void ColorWheelWidget::invalidateCache() {
    m_cachedRingPixmap = QPixmap();
    m_cachedRingWidgetSize = QSize();
    m_cachedSvImage = QImage();
    m_cachedSvHue = -1.0f;
    m_cachedSvSize = QSize();
}

QColor ColorWheelWidget::currentColor() const {
    return QColor::fromHsvF(m_hue / 360.0f, m_saturation, m_value);
}

void ColorWheelWidget::setColor(const QColor& color) {
    if (color.isValid()) {
        float newHue = color.hueF() >= 0 ? color.hueF() * 360.0f : 0.0f;
        float newSat = color.saturationF();
        float newVal = color.valueF();
        if (std::abs(newHue - m_hue) > 0.01f ||
            std::abs(newSat - m_saturation) > 0.001f ||
            std::abs(newVal - m_value) > 0.001f) {
            m_hue = newHue;
            m_saturation = newSat;
            m_value = newVal;
            update();
            emit colorChanged(currentColor());
        }
    }
}

QRectF ColorWheelWidget::getInnerSquare() const {
    qreal side = std::min(width(), height());
    qreal radius = side * 0.5;
    qreal innerR = radius * 0.72; // 外周リングの内径
    qreal boxSide = innerR * std::sqrt(2.0) * 0.82; // 内接正方形 (リングとの間にほどよい隙間)
    QPointF center(width() * 0.5, height() * 0.5);
    return QRectF(center.x() - boxSide * 0.5, center.y() - boxSide * 0.5, boxSide, boxSide);
}

void ColorWheelWidget::drawSaiRingMarker(QPainter& painter, const QPointF& pos) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 現在選択中の色相 + 180° (補色)
    float compHue = std::fmod(m_hue + 180.0f, 360.0f);
    if (compHue < 0.0f) compHue += 360.0f;

    const auto& filterMgr = DisplayFilterManager::instance();
    QColor compColor = filterMgr.applyFilter(QColor::fromHsvF(compHue / 360.0f, 1.0f, 0.95f));
    QColor compColorDark = filterMgr.applyFilter(QColor::fromHsvF(compHue / 360.0f, 1.0f, 0.45f));

    // 視認性の高い補色二重円環 (サイズを2倍: 半径 7.0)
    painter.setPen(QPen(compColorDark, 2.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(pos, 7.0, 7.0);

    painter.setPen(QPen(compColor, 1.6));
    painter.drawEllipse(pos, 7.0, 7.0);

    painter.restore();
}

void ColorWheelWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto& filterMgr = DisplayFilterManager::instance();

    QPointF center(width() * 0.5, height() * 0.5);
    qreal side = std::min(width(), height());
    qreal outerR = side * 0.5 - 3.0;
    qreal innerR = outerR * 0.73;

    // 1. SAI風 外周色相リングの描画 (静的キャッシュ)
    if (m_cachedRingPixmap.isNull() || m_cachedRingWidgetSize != size()) {
        m_cachedRingPixmap = QPixmap(size());
        m_cachedRingPixmap.fill(Qt::transparent);
        m_cachedRingWidgetSize = size();

        QPainter rp(&m_cachedRingPixmap);
        rp.setRenderHint(QPainter::Antialiasing, true);

        QConicalGradient cGrad(center, 180.0);
        for (const auto& pair : s_map) {
            double pos = (360.0 - pair.angle) / 360.0;
            if (pos >= 1.0) pos = 0.0;
            cGrad.setColorAt(pos, filterMgr.applyFilter(QColor::fromHsvF(pair.hue / 360.0f, 1.0f, 1.0f)));
        }
        cGrad.setColorAt(1.0, filterMgr.applyFilter(QColor::fromHsvF(0.0f, 1.0f, 1.0f))); // 9時 (赤)

        QPainterPath ringPath;
        ringPath.setFillRule(Qt::OddEvenFill);
        ringPath.addEllipse(center, outerR, outerR);
        ringPath.addEllipse(center, innerR, innerR);

        rp.fillPath(ringPath, cGrad);

        // 外周リングの境界線
        rp.setBrush(Qt::NoBrush);
        rp.setPen(QPen(QColor(180, 180, 180), 1.0));
        rp.drawEllipse(center, outerR, outerR);
        rp.setPen(QPen(QColor(190, 190, 190), 1.0));
        rp.drawEllipse(center, innerR, innerR);
    }

    painter.drawPixmap(0, 0, m_cachedRingPixmap);

    // 2. 内側のSV正方形描画 (色相が変わらなければ画像再生成をスキップ)
    QRectF svRect = getInnerSquare();
    int iw = static_cast<int>(svRect.width());
    int ih = static_cast<int>(svRect.height());
    if (iw > 0 && ih > 0) {
        if (m_cachedSvImage.size() != QSize(iw, ih) || std::abs(m_cachedSvHue - m_hue) > 0.001f) {
            m_cachedSvImage = QImage(iw, ih, QImage::Format_RGB32);
            m_cachedSvHue = m_hue;
            m_cachedSvSize = QSize(iw, ih);

            float h = m_hue / 360.0f;
            for (int y = 0; y < ih; ++y) {
                float v = 1.0f - static_cast<float>(y) / (ih > 1 ? (ih - 1) : 1);
                uint32_t* line = reinterpret_cast<uint32_t*>(m_cachedSvImage.scanLine(y));
                for (int x = 0; x < iw; ++x) {
                    float s = static_cast<float>(x) / (iw > 1 ? (iw - 1) : 1);
                    QColor c = QColor::fromHsvF(h, s, v);
                    line[x] = c.rgb();
                }
            }
            m_cachedSvImage = filterMgr.applyFilter(m_cachedSvImage);
        }

        painter.drawImage(svRect.topLeft(), m_cachedSvImage);

        // 正方形の外枠線
        painter.setPen(QPen(QColor(120, 120, 120), 1.0));
        painter.drawRect(svRect);
    }

    // 3. SAI風 色相リング上の選択マーカー
    double curWheelAngle = hueToWheelAngle(m_hue);
    double rad = (180.0 + curWheelAngle) * M_PI / 180.0;
    double midR = (outerR + innerR) * 0.5;
    QPointF hueMarker(center.x() + midR * std::cos(rad), center.y() + midR * std::sin(rad));
    drawSaiRingMarker(painter, hueMarker);

    // 4. SAI風 SV正方形内の選択マーカー
    qreal svX = svRect.left() + m_saturation * svRect.width();
    qreal svY = svRect.top() + (1.0f - m_value) * svRect.height();
    drawSaiRingMarker(painter, QPointF(svX, svY));
}

void ColorWheelWidget::handleMousePos(const QPointF& pos) {
    QPointF center(width() * 0.5, height() * 0.5);
    qreal dx = pos.x() - center.x();
    qreal dy = pos.y() - center.y();
    qreal dist = std::hypot(dx, dy);

    QRectF svRect = getInnerSquare();

    QColor oldColor = currentColor();

    if (m_dragMode == DragMode::HueRing || (m_dragMode == DragMode::None && dist >= svRect.width() * 0.55)) {
        m_dragMode = DragMode::HueRing;

        // 3時が0°、時計回り角度
        float standardAngle = std::atan2(dy, dx) * 180.0 / M_PI;
        // 9時を0°とした時計回り角度 (0〜360°)
        float wheelAngle = standardAngle - 180.0f;
        if (wheelAngle < 0.0f) wheelAngle += 360.0f;

        float newHue = static_cast<float>(wheelAngleToHue(wheelAngle));
        if (std::abs(newHue - m_hue) > 0.01f) {
            m_hue = newHue;
            update();
            QColor newColor = currentColor();
            if (newColor != oldColor) {
                emit colorChanged(newColor);
            }
        }
    } else {
        m_dragMode = DragMode::SVBox;
        float newSat = std::clamp(static_cast<float>((pos.x() - svRect.left()) / svRect.width()), 0.0f, 1.0f);
        float newVal = std::clamp(static_cast<float>(1.0f - (pos.y() - svRect.top()) / svRect.height()), 0.0f, 1.0f);
        if (std::abs(newSat - m_saturation) > 0.0005f || std::abs(newVal - m_value) > 0.0005f) {
            m_saturation = newSat;
            m_value = newVal;
            update();
            QColor newColor = currentColor();
            if (newColor != oldColor) {
                emit colorChanged(newColor);
            }
        }
    }
}

void ColorWheelWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        handleMousePos(event->position());
    }
}

void ColorWheelWidget::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        handleMousePos(event->position());
    }
}

void ColorWheelWidget::mouseReleaseEvent(QMouseEvent* /*event*/) {
    m_dragMode = DragMode::None;
}

void ColorWheelWidget::tabletEvent(QTabletEvent* event) {
    if (event->type() == QEvent::TabletPress) {
        handleMousePos(event->position());
        event->accept();
    } else if (event->type() == QEvent::TabletMove) {
        if (m_dragMode != DragMode::None) {
            handleMousePos(event->position());
        }
        event->accept();
    } else if (event->type() == QEvent::TabletRelease) {
        m_dragMode = DragMode::None;
        event->accept();
    }
}

} // namespace Kaburagi
