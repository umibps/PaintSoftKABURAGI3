#include "PreviewWindow.hpp"
#include "Document.hpp"
#include "DisplayFilterManager.hpp"
#include <QPainter>
#include <QCloseEvent>
#include <QResizeEvent>
#include <algorithm>

namespace Kaburagi {

PreviewWindow::PreviewWindow(QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::WindowStaysOnTopHint)
{
    setWindowTitle(tr("プレビュー"));
    resize(320, 240);
    setAttribute(Qt::WA_DeleteOnClose, false);

    // 表示フィルターの変更を監視
    connect(&DisplayFilterManager::instance(), &DisplayFilterManager::filterChanged,
            this, &PreviewWindow::refreshPreview);
}

PreviewWindow::~PreviewWindow() {
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
    }
}

void PreviewWindow::setDocument(Document* doc) {
    if (m_document == doc) return;

    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
    }

    m_document = doc;
    m_dirty = true;

    if (m_document) {
        connect(m_document, &Document::documentUpdated, this, [this](const QRect&) {
            m_dirty = true;
            update();
        });
        connect(m_document, &Document::layerListChanged, this, [this]() {
            m_dirty = true;
            update();
        });
        connect(m_document, &Document::backgroundColorChanged, this, [this]() {
            update();
        });
    }

    update();
}

void PreviewWindow::refreshPreview() {
    update();
}

void PreviewWindow::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    // ウィンドウ背景 (暗いグレー)
    painter.fillRect(rect(), QColor(40, 40, 40));

    if (!m_document || m_document->width() <= 0 || m_document->height() <= 0) {
        painter.setPen(QColor(160, 160, 160));
        painter.drawText(rect(), Qt::AlignCenter, tr("キャンバスがありません"));
        return;
    }

    int docW = m_document->width();
    int docH = m_document->height();

    // ウィンドウ領域に縦横比を保持して収まる矩形を計算 (余白マージン 8px)
    int margin = 8;
    int availW = std::max(1, width() - margin * 2);
    int availH = std::max(1, height() - margin * 2);

    double scaleW = static_cast<double>(availW) / docW;
    double scaleH = static_cast<double>(availH) / docH;
    double scale = std::min(scaleW, scaleH);

    int targetW = std::max(1, static_cast<int>(docW * scale));
    int targetH = std::max(1, static_cast<int>(docH * scale));
    int targetX = (width() - targetW) / 2;
    int targetY = (height() - targetH) / 2;
    QRect targetRect(targetX, targetY, targetW, targetH);

    // ドロップシャドウ
    painter.fillRect(targetRect.adjusted(2, 2, 2, 2), QColor(20, 20, 20, 160));

    // 表示フィルターマネージャー
    const auto& filterMgr = DisplayFilterManager::instance();
    bool hasDisplayFilter = (filterMgr.filterType() != DisplayFilterType::None);

    // 用紙背景色 (フィルター適用)
    QColor paperColor = m_document->primaryBackgroundColor();
    if (hasDisplayFilter) {
        paperColor = filterMgr.applyFilter(paperColor);
    }
    painter.fillRect(targetRect, paperColor);

    // 合成画像のキャッシュ更新
    if (m_dirty || m_cachedComposite.isNull() || m_cachedComposite.size() != QSize(docW, docH)) {
        m_cachedComposite = m_document->compositeBuffer().toQImage();
        m_dirty = false;
    }

    if (!m_cachedComposite.isNull()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        if (hasDisplayFilter) {
            // targetRect のサイズに縮小してからフィルター適用
            QImage scaled = m_cachedComposite.scaled(targetW, targetH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            painter.drawImage(targetRect, filterMgr.applyFilter(scaled));
        } else {
            painter.drawImage(targetRect, m_cachedComposite);
        }
    }

    // キャンバス境界外枠線
    painter.setPen(QColor(85, 85, 85));
    painter.drawRect(targetRect);
}

void PreviewWindow::closeEvent(QCloseEvent* event) {
    emit windowClosed();
    QWidget::closeEvent(event);
}

void PreviewWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

} // namespace Kaburagi
