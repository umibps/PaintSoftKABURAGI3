#include "ColorBoxWidget.hpp"
#include "DisplayFilterManager.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QPainterPath>

namespace Kaburagi {

ColorBoxWidget::ColorBoxWidget(QWidget* parent)
    : QWidget(parent) {
    setToolTip("描画色 / 背景色 (クリックで選択、矢印または[X]キーで入れ替え)");
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(&DisplayFilterManager::instance(), &DisplayFilterManager::filterChanged, this, QOverload<>::of(&QWidget::update));
}

QRect ColorBoxWidget::primaryRect() const {
    return QRect(4, 4, 26, 26);
}

QRect ColorBoxWidget::secondaryRect() const {
    return QRect(18, 18, 26, 26);
}

QRect ColorBoxWidget::swapButtonRect() const {
    return QRect(36, 2, 16, 16);
}

void ColorBoxWidget::setPrimaryColor(const QColor& color) {
    if (m_primaryColor == color) return;
    m_primaryColor = color;
    update();
    emit primaryColorChanged(m_primaryColor);
    if (m_isPrimaryActive) {
        emit activeColorChanged(m_primaryColor);
    }
}

void ColorBoxWidget::setSecondaryColor(const QColor& color) {
    if (m_secondaryColor == color) return;
    m_secondaryColor = color;
    update();
    emit secondaryColorChanged(m_secondaryColor);
    if (!m_isPrimaryActive) {
        emit activeColorChanged(m_secondaryColor);
    }
}

void ColorBoxWidget::setCurrentActiveColor(const QColor& color) {
    if (m_isPrimaryActive) {
        setPrimaryColor(color);
    } else {
        setSecondaryColor(color);
    }
}

void ColorBoxWidget::swapColors() {
    std::swap(m_primaryColor, m_secondaryColor);
    update();
    emit primaryColorChanged(m_primaryColor);
    emit secondaryColorChanged(m_secondaryColor);
    emit activeColorChanged(activeColor());
}

void ColorBoxWidget::resetToDefaultColors() {
    m_primaryColor = QColor(0, 0, 0);
    m_secondaryColor = QColor(255, 255, 255);
    update();
    emit primaryColorChanged(m_primaryColor);
    emit secondaryColorChanged(m_secondaryColor);
    emit activeColorChanged(activeColor());
}

void ColorBoxWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRect rSec = secondaryRect();
    QRect rPri = primaryRect();
    QRect rSwap = swapButtonRect();

    // 1. スワップボタン (右上の双方向カーブ矢印)
    painter.save();
    painter.setPen(QPen(QColor(80, 80, 80), 1.2));
    painter.setBrush(Qt::NoBrush);

    // ⤹⤸ 風の2本の矢印
    // 上の矢印 (左から右へ)
    painter.drawLine(rSwap.left() + 2, rSwap.top() + 6, rSwap.right() - 3, rSwap.top() + 3);
    painter.drawLine(rSwap.right() - 3, rSwap.top() + 3, rSwap.right() - 6, rSwap.top() + 2);
    painter.drawLine(rSwap.right() - 3, rSwap.top() + 3, rSwap.right() - 4, rSwap.top() + 6);

    // 下の矢印 (右から左へ)
    painter.drawLine(rSwap.right() - 2, rSwap.bottom() - 4, rSwap.left() + 3, rSwap.bottom() - 1);
    painter.drawLine(rSwap.left() + 3, rSwap.bottom() - 1, rSwap.left() + 6, rSwap.bottom());
    painter.drawLine(rSwap.left() + 3, rSwap.bottom() - 1, rSwap.left() + 4, rSwap.bottom() - 4);
    painter.restore();

    // 2. 描画順序: 非アクティブな方を下に、アクティブな方を上に描画
    auto drawColorBox = [&](const QRect& r, const QColor& color, bool isActive) {
        // 白黒チェック模様の背景 (透明色対応の下地)
        int step = 4;
        for (int y = r.top(); y < r.bottom(); y += step) {
            for (int x = r.left(); x < r.right(); x += step) {
                bool alt = ((x / step) + (y / step)) % 2 == 0;
                painter.fillRect(QRect(x, y, std::min(step, r.right() - x), std::min(step, r.bottom() - y)),
                                 alt ? QColor(220, 220, 220) : QColor(255, 255, 255));
            }
        }

        // カラーの塗りつぶし
        painter.fillRect(r, DisplayFilterManager::instance().applyFilter(color));

        // 枠線
        if (isActive) {
            // アクティブ時はSAI風の二重枠線 (外枠黒、内枠白)
            painter.setPen(QPen(QColor(40, 40, 40), 1.5));
            painter.drawRect(r);
            painter.setPen(QPen(Qt::white, 1.0));
            painter.drawRect(r.adjusted(1, 1, -1, -1));
        } else {
            painter.setPen(QPen(QColor(120, 120, 120), 1.0));
            painter.drawRect(r);
        }
    };

    if (m_isPrimaryActive) {
        drawColorBox(rSec, m_secondaryColor, false);
        drawColorBox(rPri, m_primaryColor, true);
    } else {
        drawColorBox(rPri, m_primaryColor, false);
        drawColorBox(rSec, m_secondaryColor, true);
    }
}

void ColorBoxWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;

    QPoint pos = event->pos();
    if (swapButtonRect().contains(pos)) {
        swapColors();
        return;
    }

    if (m_isPrimaryActive) {
        // Primaryが前面
        if (primaryRect().contains(pos)) {
            // 既にPrimaryがアクティブ
        } else if (secondaryRect().contains(pos)) {
            m_isPrimaryActive = false;
            update();
            emit activeTargetChanged(false);
            emit activeColorChanged(m_secondaryColor);
        }
    } else {
        // Secondaryが前面
        if (secondaryRect().contains(pos)) {
            // 既にSecondaryがアクティブ
        } else if (primaryRect().contains(pos)) {
            m_isPrimaryActive = true;
            update();
            emit activeTargetChanged(true);
            emit activeColorChanged(m_primaryColor);
        }
    }
}

} // namespace Kaburagi
