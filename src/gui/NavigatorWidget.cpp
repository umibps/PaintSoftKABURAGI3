#include "NavigatorWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>

#ifndef M_PI
constexpr double M_PI = 3.14159265358979323846;
#endif

namespace Kaburagi {

class NavPreviewWidget : public QWidget {
public:
    explicit NavPreviewWidget(NavigatorWidget* nav)
        : QWidget(nav), m_nav(nav) {
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumHeight(60);
    }

    QRect previewArea() const {
        return rect().adjusted(2, 2, -2, -2);
    }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    NavigatorWidget* m_nav = nullptr;
};

NavigatorWidget::NavigatorWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // キャンバス表示プレビューウィジェット (可能な限り広げる stretch 1)
    m_previewWidget = new NavPreviewWidget(this);
    mainLayout->addWidget(m_previewWidget, 1);

    // 表示拡大倍率 (SpinScale: 5% 〜 400%, 0.1%刻み, デフォルト100%)
    m_zoomScale = new SpinScale("倍率", 5.0, 400.0, 1.0, 1, "%", this);
    m_zoomScale->setValue(100.0);
    mainLayout->addWidget(m_zoomScale, 0);

    // プリセット倍率ボタン (50%, 100%, 200%)
    auto presetZoomLayout = new QHBoxLayout();
    presetZoomLayout->setSpacing(2);
    presetZoomLayout->setContentsMargins(0, 0, 0, 0);

    m_btnZoom50 = new QPushButton("50%", this);
    m_btnZoom50->setFixedHeight(20);
    m_btnZoom50->setToolTip("表示倍率を50%に変更");
    presetZoomLayout->addWidget(m_btnZoom50);

    m_btnZoom100 = new QPushButton("100%", this);
    m_btnZoom100->setFixedHeight(20);
    m_btnZoom100->setToolTip("表示倍率を100%（原寸）に変更");
    presetZoomLayout->addWidget(m_btnZoom100);

    m_btnZoom200 = new QPushButton("200%", this);
    m_btnZoom200->setFixedHeight(20);
    m_btnZoom200->setToolTip("表示倍率を200%に変更");
    presetZoomLayout->addWidget(m_btnZoom200);

    mainLayout->addLayout(presetZoomLayout);

    // 表示回転角 (SpinScale: -180° 〜 180°, 0.1°刻み, デフォルト0°)
    m_rotScale = new SpinScale("回転", -180.0, 180.0, 1.0, 1, "°", this);
    m_rotScale->setValue(0.0);
    mainLayout->addWidget(m_rotScale, 0);

    // クイック操作ボタン群 (-, +, ↺, ↻, 0°, 反転)
    auto btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(2);
    btnLayout->setContentsMargins(0, 0, 0, 0);

    m_btnZoomOut = new QPushButton("-", this);
    m_btnZoomOut->setFixedSize(22, 20);
    m_btnZoomOut->setToolTip("ズームアウト");

    m_btnZoomIn = new QPushButton("+", this);
    m_btnZoomIn->setFixedSize(22, 20);
    m_btnZoomIn->setToolTip("ズームイン");

    m_btnRotCCW = new QPushButton(QString::fromUtf8("↺"), this);
    m_btnRotCCW->setFixedSize(22, 20);
    m_btnRotCCW->setToolTip("反時計回りに回転 (-15°)");

    m_btnRotCW = new QPushButton(QString::fromUtf8("↻"), this);
    m_btnRotCW->setFixedSize(22, 20);
    m_btnRotCW->setToolTip("時計回りに回転 (+15°)");

    m_btnResetRot = new QPushButton("0°", this);
    m_btnResetRot->setFixedSize(24, 20);
    m_btnResetRot->setToolTip("回転角度をリセット");

    m_btnFlip = new QPushButton("反転", this);
    m_btnFlip->setFixedSize(36, 20);
    m_btnFlip->setToolTip("左右反転表示");

    btnLayout->addWidget(m_btnZoomOut);
    btnLayout->addWidget(m_btnZoomIn);
    btnLayout->addSpacing(4);
    btnLayout->addWidget(m_btnRotCCW);
    btnLayout->addWidget(m_btnRotCW);
    btnLayout->addWidget(m_btnResetRot);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnFlip);
    mainLayout->addLayout(btnLayout);

    connect(m_zoomScale, &SpinScale::valueChanged, this, [this](double val) {
        emit zoomChanged(val / 100.0);
    });
    connect(m_btnZoom50, &QPushButton::clicked, this, [this]() {
        emit zoomChanged(0.5);
    });
    connect(m_btnZoom100, &QPushButton::clicked, this, [this]() {
        emit zoomChanged(1.0);
    });
    connect(m_btnZoom200, &QPushButton::clicked, this, [this]() {
        emit zoomChanged(2.0);
    });
    connect(m_rotScale, &SpinScale::valueChanged, this, [this](double val) {
        double angle = val < 0 ? val + 360.0 : val;
        emit rotationChanged(angle);
    });

    connect(m_btnZoomIn, &QPushButton::clicked, this, [this]() {
        emit zoomChanged(m_zoom * 1.2);
    });
    connect(m_btnZoomOut, &QPushButton::clicked, this, [this]() {
        emit zoomChanged(m_zoom / 1.2);
    });
    connect(m_btnRotCCW, &QPushButton::clicked, this, [this]() {
        double newAngle = std::fmod(m_angleDeg - 15.0 + 360.0, 360.0);
        emit rotationChanged(newAngle);
    });
    connect(m_btnRotCW, &QPushButton::clicked, this, [this]() {
        double newAngle = std::fmod(m_angleDeg + 15.0, 360.0);
        emit rotationChanged(newAngle);
    });
    connect(m_btnResetRot, &QPushButton::clicked, this, [this]() {
        emit rotationChanged(0.0);
    });
    connect(m_btnFlip, &QPushButton::clicked, this, [this]() {
        emit flipHorizontallyTriggered();
    });
}

QWidget* NavigatorWidget::previewWidget() const {
    return m_previewWidget;
}

void NavigatorWidget::setDocument(Document* doc) {
    if (m_document == doc) return;
    m_document = doc;
    m_previewDirty = true;
    if (m_document) {
        connect(m_document, &Document::documentUpdated, this, [this]() {
            m_previewDirty = true;
            if (m_previewWidget) m_previewWidget->update();
            update();
        });
    }
    if (m_previewWidget) m_previewWidget->update();
    update();
}

void NavigatorWidget::updateViewTransform(double zoom, double angleDeg, bool flipped, const QPointF& docCenterVisible, const QSizeF& viewportSize) {
    m_zoom = zoom;
    m_angleDeg = angleDeg;
    m_flippedHorizontally = flipped;
    m_docCenterVisible = docCenterVisible;
    m_viewportSize = viewportSize;

    if (m_zoomScale) {
        m_zoomScale->blockSignals(true);
        m_zoomScale->setValue(m_zoom * 100.0);
        m_zoomScale->blockSignals(false);
    }

    int displayAngle = static_cast<int>(std::round(m_angleDeg)) % 360;
    if (displayAngle > 180) displayAngle -= 360;

    if (m_rotScale) {
        m_rotScale->blockSignals(true);
        m_rotScale->setValue(displayAngle);
        m_rotScale->blockSignals(false);
    }
    if (m_previewWidget) m_previewWidget->update();
    update();
}

double NavigatorWidget::getNavScale() const {
    if (!m_document) return 1.0;
    int docW = m_document->width();
    int docH = m_document->height();
    if (docW <= 0 || docH <= 0) return 1.0;

    QRect previewArea = m_previewWidget ? m_previewWidget->previewArea() : QRect(6, 4, width() - 12, 76);
    double D = std::sqrt(static_cast<double>(docW * docW + docH * docH));
    if (D <= 0) return 1.0;

    double scaleX = static_cast<double>(previewArea.width() - 8) / D;
    double scaleY = static_cast<double>(previewArea.height() - 8) / D;
    return std::min(scaleX, scaleY);
}

QPointF NavigatorWidget::getNavCenter() const {
    QRect previewArea = m_previewWidget ? m_previewWidget->previewArea() : QRect(6, 4, width() - 12, 76);
    return QPointF(previewArea.center());
}

void NavigatorWidget::updateDocPreview() {
    if (!m_document) {
        m_docPreviewImage = QImage();
        return;
    }
    int docW = m_document->width();
    int docH = m_document->height();
    if (docW <= 0 || docH <= 0) {
        m_docPreviewImage = QImage();
        return;
    }

    double S_nav = getNavScale();
    int rw = std::max(1, static_cast<int>(std::round(docW * S_nav)));
    int rh = std::max(1, static_cast<int>(std::round(docH * S_nav)));

    m_docPreviewImage = QImage(rw, rh, QImage::Format_ARGB32_Premultiplied);
    m_docPreviewImage.fill(Qt::white);

    const auto& comp = m_document->compositeBuffer();
    int lastTx = -1, lastTy = -1;
    const uint32_t* tilePixels = nullptr;

    for (int y = 0; y < rh; ++y) {
        int docY = (y * docH) / rh;
        int ty = docY / TileMemoryPool::TILE_HEIGHT;
        int ly = docY % TileMemoryPool::TILE_HEIGHT;
        uint32_t* line = reinterpret_cast<uint32_t*>(m_docPreviewImage.scanLine(y));

        for (int x = 0; x < rw; ++x) {
            int docX = (x * docW) / rw;
            int tx = docX / TileMemoryPool::TILE_WIDTH;
            int lx = docX % TileMemoryPool::TILE_WIDTH;

            if (tx != lastTx || ty != lastTy) {
                lastTx = tx;
                lastTy = ty;
                const Tile* tile = comp.getTile(tx, ty);
                tilePixels = (tile && tile->isValid()) ? reinterpret_cast<const uint32_t*>(tile->readBuffer()) : nullptr;
            }

            if (!tilePixels) continue;

            uint32_t p = tilePixels[ly * TileMemoryPool::TILE_WIDTH + lx];
            uint32_t a = (p >> 24) & 0xFF;
            if (a == 0) continue;

            if (a == 255) {
                line[x] = p;
            } else {
                uint32_t r = (p >> 16) & 0xFF;
                uint32_t g = (p >> 8) & 0xFF;
                uint32_t b = p & 0xFF;
                float alpha = a / 255.0f;
                uint32_t outR = static_cast<uint32_t>(r * alpha + 255 * (1.0f - alpha) + 0.5f);
                uint32_t outG = static_cast<uint32_t>(g * alpha + 255 * (1.0f - alpha) + 0.5f);
                uint32_t outB = static_cast<uint32_t>(b * alpha + 255 * (1.0f - alpha) + 0.5f);
                line[x] = 0xFF000000 | (outR << 16) | (outG << 8) | outB;
            }
        }
    }
}

QPointF NavigatorWidget::previewPosToDocPos(const QPointF& viewPt) const {
    if (!m_document) return QPointF();
    int docW = m_document->width();
    int docH = m_document->height();
    if (docW <= 0 || docH <= 0) return QPointF();

    double S_nav = getNavScale();
    if (S_nav <= 0) return QPointF();
    QPointF C_nav = getNavCenter();

    // 1. ナビゲータープレビュー中心からの相対ベクトル
    QPointF delta = viewPt - C_nav;

    // 2. 逆回転 (-m_angleDeg)
    double rad = -m_angleDeg * M_PI / 180.0;
    double cosA = std::cos(rad);
    double sinA = std::sin(rad);
    QPointF unrot(
        delta.x() * cosA - delta.y() * sinA,
        delta.x() * sinA + delta.y() * cosA
    );

    // 3. 逆反転 (左右反転していたらX座標を反転)
    if (m_flippedHorizontally) {
        unrot.setX(-unrot.x());
    }

    // 4. ナビゲータースケールで割ってドキュメント中心を加算
    double docX = (unrot.x() / S_nav) + (docW * 0.5);
    double docY = (unrot.y() / S_nav) + (docH * 0.5);

    docX = std::clamp(docX, 0.0, static_cast<double>(docW));
    docY = std::clamp(docY, 0.0, static_cast<double>(docH));
    return QPointF(docX, docY);
}

void NavigatorWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    m_previewDirty = true;
    if (m_previewWidget) {
        m_previewWidget->update();
    }
}

void NavPreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_nav) {
        m_nav->m_previewDirty = true;
        update();
    }
}

void NavPreviewWidget::mousePressEvent(QMouseEvent* event) {
    if (!m_nav) return;
    if (event->button() == Qt::LeftButton) {
        QRect area = previewArea();
        if (area.contains(event->pos())) {
            m_nav->m_isDraggingPreview = true;
            setCursor(Qt::ClosedHandCursor);
            QPointF docPt = m_nav->previewPosToDocPos(event->position());
            emit m_nav->viewCenterChanged(docPt);
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void NavPreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!m_nav) return;
    if (m_nav->m_isDraggingPreview && (event->buttons() & Qt::LeftButton)) {
        QPointF docPt = m_nav->previewPosToDocPos(event->position());
        emit m_nav->viewCenterChanged(docPt);
        event->accept();
        return;
    }

    QRect area = previewArea();
    if (area.contains(event->pos())) {
        setCursor(Qt::PointingHandCursor);
    } else {
        unsetCursor();
    }
    QWidget::mouseMoveEvent(event);
}

void NavPreviewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (!m_nav) return;
    if (m_nav->m_isDraggingPreview && event->button() == Qt::LeftButton) {
        m_nav->m_isDraggingPreview = false;
        QRect area = previewArea();
        if (area.contains(event->pos())) {
            setCursor(Qt::PointingHandCursor);
        } else {
            unsetCursor();
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void NavPreviewWidget::paintEvent(QPaintEvent* /*event*/) {
    if (!m_nav) return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QRect area = previewArea();
    painter.fillRect(area, QColor(220, 220, 220));
    painter.setPen(QColor(160, 160, 160));
    painter.drawRect(area);

    if (!m_nav->m_document) return;

    int docW = m_nav->m_document->width();
    int docH = m_nav->m_document->height();
    if (docW <= 0 || docH <= 0) return;

    if (m_nav->m_previewDirty || m_nav->m_docPreviewImage.isNull()) {
        m_nav->updateDocPreview();
        m_nav->m_previewDirty = false;
    }

    double S_nav = m_nav->getNavScale();
    QPointF C_nav = m_nav->getNavCenter();
    double drawW = docW * S_nav;
    double drawH = docH * S_nav;

    // 1. キャンバスの描画: 現在の表示回転角・左右反転に合わせてプレビューエリア中央で回転描画
    painter.save();
    painter.setClipRect(area);
    painter.translate(C_nav);
    painter.rotate(m_nav->m_angleDeg);
    if (m_nav->m_flippedHorizontally) {
        painter.scale(-1.0, 1.0);
    }

    QRectF docRect(-drawW * 0.5, -drawH * 0.5, drawW, drawH);
    painter.fillRect(docRect, Qt::white);
    if (!m_nav->m_docPreviewImage.isNull()) {
        painter.drawImage(docRect, m_nav->m_docPreviewImage);
    }
    painter.setPen(QColor(100, 100, 100));
    painter.drawRect(docRect);

    painter.restore();

    // 2. 赤枠（ビューポート枠）の描画: 枠は回転せず水平矩形として表示
    if (m_nav->m_viewportSize.width() > 0 && m_nav->m_viewportSize.height() > 0 && m_nav->m_zoom > 0) {
        QPointF docCenterPt(docW * 0.5, docH * 0.5);
        QPointF deltaDoc = m_nav->m_docCenterVisible - docCenterPt;

        // ドキュメント上の相対オフセットをナビゲーター上のオフセットに変換（スケール・反転・回転）
        QPointF deltaNav(deltaDoc.x() * S_nav, deltaDoc.y() * S_nav);
        if (m_nav->m_flippedHorizontally) {
            deltaNav.setX(-deltaNav.x());
        }
        double rad = m_nav->m_angleDeg * M_PI / 180.0;
        double cosA = std::cos(rad);
        double sinA = std::sin(rad);
        QPointF rotatedDelta(
            deltaNav.x() * cosA - deltaNav.y() * sinA,
            deltaNav.x() * sinA + deltaNav.y() * cosA
        );
        QPointF redBoxCenter = C_nav + rotatedDelta;

        // 赤枠サイズ（画面サイズを ナビゲータースケール / ズーム で変換）
        double boxW = m_nav->m_viewportSize.width() * (S_nav / m_nav->m_zoom);
        double boxH = m_nav->m_viewportSize.height() * (S_nav / m_nav->m_zoom);
        QRectF redBox(redBoxCenter.x() - boxW * 0.5,
                      redBoxCenter.y() - boxH * 0.5,
                      boxW, boxH);

        painter.save();
        painter.setClipRect(area);
        painter.setPen(QPen(QColor(220, 20, 20), 1.5));
        painter.setBrush(QColor(255, 0, 0, 25)); // 薄い赤半透明
        painter.drawRect(redBox);
        painter.restore();
    }
}

} // namespace Kaburagi
