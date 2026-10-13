#include "CanvasContainer.hpp"
#include <algorithm>
#include <cmath>

namespace Kaburagi {

CanvasContainer::CanvasContainer(CanvasWidget* canvas, QWidget* parent)
    : QWidget(parent)
    , m_canvas(canvas) {
    auto grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);

    m_hScrollBar = new QScrollBar(Qt::Horizontal, this);
    m_vScrollBar = new QScrollBar(Qt::Vertical, this);
    m_cornerWidget = new QWidget(this);

    // スタイル設定（SAI風のクリーンなスクロールバー）
    m_cornerWidget->setStyleSheet("background-color: #e0e0e0;");

    grid->addWidget(m_canvas, 0, 0);
    grid->addWidget(m_vScrollBar, 0, 1);
    grid->addWidget(m_hScrollBar, 1, 0);
    grid->addWidget(m_cornerWidget, 1, 1);

    m_hScrollBar->setVisible(false);
    m_vScrollBar->setVisible(false);
    m_cornerWidget->setVisible(false);

    connect(m_canvas, &CanvasWidget::transformChanged, this, &CanvasContainer::updateScrollBars);
    connect(m_hScrollBar, &QScrollBar::valueChanged, this, &CanvasContainer::onHScrollBarValueChanged);
    connect(m_vScrollBar, &QScrollBar::valueChanged, this, &CanvasContainer::onVScrollBarValueChanged);
}

void CanvasContainer::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateScrollBars();
}

void CanvasContainer::updateScrollBars() {
    if (m_isUpdating || !m_canvas || !m_canvas->document()) return;
    m_isUpdating = true;

    int docW = m_canvas->document()->width();
    int docH = m_canvas->document()->height();
    double W_view = m_canvas->width();
    double H_view = m_canvas->height();

    if (docW <= 0 || docH <= 0 || W_view <= 0 || H_view <= 0) {
        m_hScrollBar->setVisible(false);
        m_vScrollBar->setVisible(false);
        m_cornerWidget->setVisible(false);
        m_isUpdating = false;
        return;
    }

    // 画面上での用紙4頂点のバウンディングボックスを取得
    QPointF p0 = m_canvas->docToView(QPointF(0, 0));
    QPointF p1 = m_canvas->docToView(QPointF(docW, 0));
    QPointF p2 = m_canvas->docToView(QPointF(docW, docH));
    QPointF p3 = m_canvas->docToView(QPointF(0, docH));

    double paperW = std::max({p0.x(), p1.x(), p2.x(), p3.x()}) - std::min({p0.x(), p1.x(), p2.x(), p3.x()});
    double paperH = std::max({p0.y(), p1.y(), p2.y(), p3.y()}) - std::min({p0.y(), p1.y(), p2.y(), p3.y()});

    // キャンバスがウィジェット表示領域より大きい場合にのみスクロールバーを表示
    bool needH = (paperW > W_view + 2.0);
    bool needV = (paperH > H_view + 2.0);

    m_hScrollBar->setVisible(needH);
    m_vScrollBar->setVisible(needV);
    m_cornerWidget->setVisible(needH && needV);

    // ナビゲーションと同じ可動範囲（画面中央に表示可能なドキュメント座標 [0, docW] x [0, docH]）
    QPointF C_view(W_view * 0.5, H_view * 0.5);
    QPointF docCenter = m_canvas->viewToDoc(C_view);
    double zoom = m_canvas->zoom();

    if (needH) {
        m_hScrollBar->blockSignals(true);
        m_hScrollBar->setRange(0, docW);
        int pageStepX = (zoom > 0) ? std::max(1, static_cast<int>(std::round(W_view / zoom))) : docW;
        m_hScrollBar->setPageStep(pageStepX);
        int curValX = std::clamp(static_cast<int>(std::round(docCenter.x())), 0, docW);
        m_hScrollBar->setValue(curValX);
        m_hScrollBar->blockSignals(false);
    }

    if (needV) {
        m_vScrollBar->blockSignals(true);
        m_vScrollBar->setRange(0, docH);
        int pageStepY = (zoom > 0) ? std::max(1, static_cast<int>(std::round(H_view / zoom))) : docH;
        m_vScrollBar->setPageStep(pageStepY);
        int curValY = std::clamp(static_cast<int>(std::round(docCenter.y())), 0, docH);
        m_vScrollBar->setValue(curValY);
        m_vScrollBar->blockSignals(false);
    }

    m_isUpdating = false;
}

void CanvasContainer::onHScrollBarValueChanged(int value) {
    if (m_isUpdating || !m_canvas || !m_canvas->document()) return;
    m_isUpdating = true;

    double W_view = m_canvas->width();
    double H_view = m_canvas->height();
    QPointF C_view(W_view * 0.5, H_view * 0.5);
    QPointF currentDocCenter = m_canvas->viewToDoc(C_view);

    int docW = m_canvas->document()->width();
    double targetDocX = std::clamp(static_cast<double>(value), 0.0, static_cast<double>(docW));

    m_canvas->setViewCenter(QPointF(targetDocX, currentDocCenter.y()));

    m_isUpdating = false;
}

void CanvasContainer::onVScrollBarValueChanged(int value) {
    if (m_isUpdating || !m_canvas || !m_canvas->document()) return;
    m_isUpdating = true;

    double W_view = m_canvas->width();
    double H_view = m_canvas->height();
    QPointF C_view(W_view * 0.5, H_view * 0.5);
    QPointF currentDocCenter = m_canvas->viewToDoc(C_view);

    int docH = m_canvas->document()->height();
    double targetDocY = std::clamp(static_cast<double>(value), 0.0, static_cast<double>(docH));

    m_canvas->setViewCenter(QPointF(currentDocCenter.x(), targetDocY));

    m_isUpdating = false;
}

} // namespace Kaburagi
