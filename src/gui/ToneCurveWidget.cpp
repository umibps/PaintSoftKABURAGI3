#include "ToneCurveWidget.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <algorithm>
#include <cmath>

namespace Kaburagi {

ToneCurveWidget::ToneCurveWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(180, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    resetAllCurves();
}

void ToneCurveWidget::resetAllCurves() {
    for (size_t i = 0; i < 4; ++i) {
        m_points[i] = { QPointF(0.0, 0.0), QPointF(255.0, 255.0) };
    }
    m_selectedPointIdx = -1;
    m_hoverPointIdx = -1;
    update();
}

void ToneCurveWidget::resetCurrentCurve() {
    size_t idx = static_cast<size_t>(m_channel);
    m_points[idx] = { QPointF(0.0, 0.0), QPointF(255.0, 255.0) };
    m_selectedPointIdx = -1;
    m_hoverPointIdx = -1;
    update();
    emit curveChanged(m_channel, m_points[idx]);
}

void ToneCurveWidget::setCurvePoints(AdjustmentChannel ch, const std::vector<QPointF>& points) {
    size_t idx = static_cast<size_t>(ch);
    if (points.size() >= 2) {
        m_points[idx] = points;
    } else {
        m_points[idx] = { QPointF(0.0, 0.0), QPointF(255.0, 255.0) };
    }
    update();
}

const std::vector<QPointF>& ToneCurveWidget::curvePoints(AdjustmentChannel ch) const {
    return m_points[static_cast<size_t>(ch)];
}

void ToneCurveWidget::setCurrentChannel(AdjustmentChannel ch) {
    if (m_channel == ch) return;
    m_channel = ch;
    m_selectedPointIdx = -1;
    m_hoverPointIdx = -1;
    update();
}

void ToneCurveWidget::setInterpolationMode(CurveInterpolationMode mode) {
    if (m_interpMode == mode) return;
    m_interpMode = mode;
    update();
}

void ToneCurveWidget::setHistogram(const std::array<int, 256>& hist) {
    m_histogram = hist;
    m_maxHistCount = 1;
    for (int count : m_histogram) {
        if (count > m_maxHistCount) m_maxHistCount = count;
    }
    update();
}

QRectF ToneCurveWidget::curveAreaRect() const {
    const qreal margin = 8.0;
    qreal w = width() - margin * 2.0;
    qreal h = height() - margin * 2.0;
    qreal size = std::max(40.0, std::min(w, h));
    qreal x = (width() - size) * 0.5;
    qreal y = (height() - size) * 0.5;
    return QRectF(x, y, size, size);
}

QPointF ToneCurveWidget::curveToWidget(const QPointF& pt) const {
    QRectF r = curveAreaRect();
    qreal wx = r.left() + (pt.x() / 255.0) * r.width();
    qreal wy = r.bottom() - (pt.y() / 255.0) * r.height();
    return QPointF(wx, wy);
}

QPointF ToneCurveWidget::widgetToCurve(const QPointF& pos) const {
    QRectF r = curveAreaRect();
    qreal cx = std::clamp((pos.x() - r.left()) / r.width() * 255.0, 0.0, 255.0);
    qreal cy = std::clamp((r.bottom() - pos.y()) / r.height() * 255.0, 0.0, 255.0);
    return QPointF(cx, cy);
}

int ToneCurveWidget::findHoverPoint(const QPointF& pos) const {
    const auto& pts = m_points[static_cast<size_t>(m_channel)];
    const qreal hitDist = 7.0;

    for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
        QPointF wp = curveToWidget(pts[i]);
        if (std::hypot(wp.x() - pos.x(), wp.y() - pos.y()) <= hitDist) {
            return i;
        }
    }
    return -1;
}

void ToneCurveWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRectF area = curveAreaRect();

    // 1. 背景描画
    p.fillRect(area, QColor(245, 245, 245));

    // 2. ヒストグラム描画
    if (m_maxHistCount > 0) {
        QColor histColor;
        switch (m_channel) {
        case AdjustmentChannel::Red: histColor = QColor(255, 100, 100, 80); break;
        case AdjustmentChannel::Green: histColor = QColor(100, 200, 100, 80); break;
        case AdjustmentChannel::Blue: histColor = QColor(100, 150, 255, 80); break;
        default: histColor = QColor(140, 140, 140, 70); break;
        }

        p.setPen(Qt::NoPen);
        p.setBrush(histColor);

        for (int i = 0; i < 256; ++i) {
            qreal barH = (static_cast<qreal>(m_histogram[i]) / m_maxHistCount) * area.height();
            if (barH > 0.5) {
                qreal bx = area.left() + (i / 255.0) * area.width();
                qreal bw = area.width() / 256.0 + 0.5;
                p.drawRect(QRectF(bx, area.bottom() - barH, bw, barH));
            }
        }
    }

    // 3. グリッド線 (25%刻み)
    p.setPen(QPen(QColor(210, 210, 210), 1.0, Qt::DotLine));
    for (int i = 1; i <= 3; ++i) {
        qreal t = i * 0.25;
        qreal gx = area.left() + t * area.width();
        qreal gy = area.top() + t * area.height();
        p.drawLine(QPointF(gx, area.top()), QPointF(gx, area.bottom()));
        p.drawLine(QPointF(area.left(), gy), QPointF(area.right(), gy));
    }

    // 4. 対角線 (初期状態基準線)
    p.setPen(QPen(QColor(190, 190, 190), 1.0, Qt::DashLine));
    p.drawLine(area.bottomLeft(), area.topRight());

    // 5. 外枠線
    p.setPen(QPen(QColor(130, 130, 130), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(area);

    // 6. トーンカーブ曲線の描画
    size_t chIdx = static_cast<size_t>(m_channel);
    const auto& curPts = m_points[chIdx];
    auto lut = AdjustmentLayer::calculateCurveLUT(curPts, m_interpMode);

    QColor curveColor;
    switch (m_channel) {
    case AdjustmentChannel::Red: curveColor = QColor(220, 40, 40); break;
    case AdjustmentChannel::Green: curveColor = QColor(40, 160, 40); break;
    case AdjustmentChannel::Blue: curveColor = QColor(40, 90, 220); break;
    default: curveColor = QColor(30, 30, 30); break;
    }

    p.setPen(QPen(curveColor, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPolygonF curvePoly;
    for (int x = 0; x < 256; ++x) {
        curvePoly << curveToWidget(QPointF(x, lut[x]));
    }
    p.drawPolyline(curvePoly);

    // 7. 制御点の描画
    for (int i = 0; i < static_cast<int>(curPts.size()); ++i) {
        QPointF wp = curveToWidget(curPts[i]);
        bool isSel = (i == m_selectedPointIdx);
        bool isHover = (i == m_hoverPointIdx);

        qreal r = isSel ? 4.5 : (isHover ? 4.0 : 3.0);
        p.setPen(QPen(isSel ? QColor(0, 120, 240) : (isHover ? QColor(255, 120, 0) : QColor(30, 30, 30)), 1.2));
        p.setBrush(isSel ? QBrush(QColor(180, 220, 255)) : (isHover ? QBrush(QColor(255, 230, 150)) : QBrush(Qt::white)));
        p.drawEllipse(wp, r, r);
    }
}

void ToneCurveWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QRectF area = curveAreaRect();
        int hit = findHoverPoint(event->position());
        size_t chIdx = static_cast<size_t>(m_channel);

        if (hit >= 0) {
            m_selectedPointIdx = hit;
            m_isDragging = true;
        } else if (area.adjusted(-4, -4, 4, 4).contains(event->position())) {
            // 新規制御点の追加
            QPointF newPt = widgetToCurve(event->position());
            auto& pts = m_points[chIdx];

            // 既存点と近すぎなければ挿入
            bool tooClose = false;
            for (const auto& p : pts) {
                if (std::abs(p.x() - newPt.x()) < 4.0) {
                    tooClose = true;
                    break;
                }
            }
            if (!tooClose) {
                pts.push_back(newPt);
                std::sort(pts.begin(), pts.end(), [](const QPointF& a, const QPointF& b) {
                    return a.x() < b.x();
                });
                for (size_t i = 0; i < pts.size(); ++i) {
                    if (pts[i] == newPt) {
                        m_selectedPointIdx = static_cast<int>(i);
                        break;
                    }
                }
                m_isDragging = true;
                emit curveChanged(m_channel, pts);
            }
        }
        update();
    } else if (event->button() == Qt::RightButton) {
        // 右クリックで選択中の制御点を削除 (始点・終点は保護)
        int hit = findHoverPoint(event->position());
        size_t chIdx = static_cast<size_t>(m_channel);
        auto& pts = m_points[chIdx];
        if (hit > 0 && hit < static_cast<int>(pts.size()) - 1) {
            pts.erase(pts.begin() + hit);
            m_selectedPointIdx = -1;
            m_hoverPointIdx = -1;
            update();
            emit curveChanged(m_channel, pts);
        }
    }
}

void ToneCurveWidget::mouseMoveEvent(QMouseEvent* event) {
    size_t chIdx = static_cast<size_t>(m_channel);
    auto& pts = m_points[chIdx];

    if (m_isDragging && m_selectedPointIdx >= 0 && m_selectedPointIdx < static_cast<int>(pts.size())) {
        QRectF area = curveAreaRect();
        // 枠外に大きくドラッグされたら削除 (始点・終点以外)
        if (m_selectedPointIdx > 0 && m_selectedPointIdx < static_cast<int>(pts.size()) - 1) {
            if (event->position().x() < area.left() - 25.0 || event->position().x() > area.right() + 25.0 ||
                event->position().y() < area.top() - 25.0 || event->position().y() > area.bottom() + 25.0) {
                pts.erase(pts.begin() + m_selectedPointIdx);
                m_selectedPointIdx = -1;
                m_isDragging = false;
                update();
                emit curveChanged(m_channel, pts);
                return;
            }
        }

        QPointF cur = widgetToCurve(event->position());

        // 始点は X=0 固定、終点は X=255 固定
        if (m_selectedPointIdx == 0) {
            pts[0].setY(cur.y());
        } else if (m_selectedPointIdx == static_cast<int>(pts.size()) - 1) {
            pts.back().setY(cur.y());
        } else {
            // 前後の点の間に制限
            qreal minX = pts[m_selectedPointIdx - 1].x() + 1.0;
            qreal maxX = pts[m_selectedPointIdx + 1].x() - 1.0;
            cur.setX(std::clamp(cur.x(), minX, maxX));
            pts[m_selectedPointIdx] = cur;
        }

        update();
        emit curveChanged(m_channel, pts);
    } else {
        int hover = findHoverPoint(event->position());
        if (hover != m_hoverPointIdx) {
            m_hoverPointIdx = hover;
            update();
        }
    }
}

void ToneCurveWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        update();
    }
}

void ToneCurveWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        size_t chIdx = static_cast<size_t>(m_channel);
        auto& pts = m_points[chIdx];
        if (m_selectedPointIdx > 0 && m_selectedPointIdx < static_cast<int>(pts.size()) - 1) {
            pts.erase(pts.begin() + m_selectedPointIdx);
            m_selectedPointIdx = -1;
            m_hoverPointIdx = -1;
            update();
            emit curveChanged(m_channel, pts);
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

} // namespace Kaburagi
