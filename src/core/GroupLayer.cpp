#include "GroupLayer.hpp"
#include <algorithm>
#include <cstring>
#include <QPainter>
#include <QPainterPath>
#include <QImage>

namespace Kaburagi {

GroupLayer::GroupLayer(const QString& name, int width, int height)
    : Layer(name, width, height, LayerType::Group) {
}

void GroupLayer::addChild(const LayerPtr& child) {
    if (!child) return;
    // 既に存在していなければ追加
    if (!containsChild(child)) {
        m_children.push_back(child);
        child->setParentGroup(shared_from_this());
    }
}

void GroupLayer::removeChild(const LayerPtr& child) {
    if (!child) return;
    auto it = std::remove_if(m_children.begin(), m_children.end(), [&child](const std::weak_ptr<Layer>& wp) {
        auto sp = wp.lock();
        return !sp || sp == child;
    });
    m_children.erase(it, m_children.end());
    if (child->parentGroup() == shared_from_this()) {
        child->setParentGroup(nullptr);
    }
}

void GroupLayer::clearChildren() {
    for (auto& wp : m_children) {
        auto sp = wp.lock();
        if (sp && sp->parentGroup() == shared_from_this()) {
            sp->setParentGroup(nullptr);
        }
    }
    m_children.clear();
}

bool GroupLayer::containsChild(const LayerPtr& layer) const {
    if (!layer) return false;
    for (const auto& wp : m_children) {
        auto sp = wp.lock();
        if (sp && sp == layer) {
            return true;
        }
    }
    return false;
}

void GroupLayer::clearTileBuffer(const QRect& dirtyRect) {
    QRect targetRect = dirtyRect.isEmpty() ? QRect(0, 0, m_tileManager.width(), m_tileManager.height()) : dirtyRect;

    int startTx = std::max(0, targetRect.left() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int endTx = std::min(m_tileManager.tilesX() - 1, targetRect.right() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int startTy = std::max(0, targetRect.top() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));
    int endTy = std::min(m_tileManager.tilesY() - 1, targetRect.bottom() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));

    for (int ty = startTy; ty <= endTy; ++ty) {
        for (int tx = startTx; tx <= endTx; ++tx) {
            Tile* t = m_tileManager.getTileForWrite(tx, ty);
            if (t && t->writeBuffer()) {
                std::memset(t->writeBuffer(), 0, TileMemoryPool::TILE_BYTE_SIZE);
            }
        }
    }
}

void GroupLayer::renderToTarget(TileManager& target, const QRect& dirtyRect, const Layer* baseLayer) {
    if (!m_visible || m_opacity <= 0.0f) return;

    if (m_isClippingMask) {
        if (!baseLayer || baseLayer->effectiveOpacity() <= 0.0f) {
            return;
        }
    }

    if (m_blendMode == BlendMode::PassThrough && !m_isClippingMask) {
        // 通過 (Pass-Through) モード: 子レイヤーのブレンドモードを活かしてターゲットに直接合成
        for (const auto& wp : m_children) {
            auto child = wp.lock();
            if (child && child->isVisible()) {
                child->renderToTarget(target, dirtyRect);
            }
        }
    } else {
        // 通常・乗算等の独立合成モード (またはクリッピング時): グループバッファを自身の不透明度・ブレンドモードで合成
        int startTx = std::max(0, dirtyRect.left() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
        int endTx = std::min(m_tileManager.tilesX() - 1, dirtyRect.right() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
        int startTy = std::max(0, dirtyRect.top() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));
        int endTy = std::min(m_tileManager.tilesY() - 1, dirtyRect.bottom() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));

        constexpr size_t PIXELS_PER_TILE = TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT;

        for (int ty = startTy; ty <= endTy; ++ty) {
            for (int tx = startTx; tx <= endTx; ++tx) {
                const Tile* srcTile = m_tileManager.getTile(tx, ty);
                if (!srcTile || !srcTile->readBuffer()) {
                    continue;
                }

                const Tile* maskTile = nullptr;
                if (m_isClippingMask && baseLayer) {
                    maskTile = baseLayer->tileManager().getTile(tx, ty);
                    if (!maskTile || !maskTile->readBuffer()) {
                        continue;
                    }
                }

                Tile* dstTile = target.getTileForWrite(tx, ty);
                uint32_t* dstBuf = reinterpret_cast<uint32_t*>(dstTile->writeBuffer());
                const uint32_t* srcBuf = reinterpret_cast<const uint32_t*>(srcTile->readBuffer());

                if (m_isClippingMask && maskTile) {
                    const uint32_t* maskBuf = reinterpret_cast<const uint32_t*>(maskTile->readBuffer());
                    blendTileWithClipMask(dstBuf, srcBuf, maskBuf, m_opacity, m_blendMode, PIXELS_PER_TILE, baseLayer->effectiveOpacity());
                } else {
                    blendTile(dstBuf, srcBuf, m_opacity, m_blendMode, PIXELS_PER_TILE, false);
                }
            }
        }
    }
}

void GroupLayer::translate(int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    Layer::translate(dx, dy);
    if (m_isPanelFolder) {
        m_panelPolygon.translate(dx, dy);
    }
    for (const auto& wp : m_children) {
        auto child = wp.lock();
        if (child) {
            child->translate(dx, dy);
        }
    }
}

QRect GroupLayer::contentBounds() const {
    if (m_isPanelFolder && !m_panelPolygon.isEmpty()) {
        return m_panelPolygon.boundingRect().toAlignedRect();
    }
    QRect united;
    for (const auto& wp : m_children) {
        auto child = wp.lock();
        if (child && child->isVisible()) {
            QRect cb = child->contentBounds();
            if (!cb.isEmpty()) {
                united = united.isEmpty() ? cb : united.united(cb);
            }
        }
    }
    return united;
}

void GroupLayer::applyPanelMaskAndBorder(const QRect& dirtyRect) {
    if (!m_isPanelFolder || m_panelPolygon.isEmpty()) return;

    QRect targetRect = dirtyRect.isEmpty() ? QRect(0, 0, m_tileManager.width(), m_tileManager.height()) : dirtyRect;

    QPainterPath panelPath;
    panelPath.addPolygon(m_panelPolygon);
    panelPath.closeSubpath();

    QRect polyBounding = m_panelPolygon.boundingRect().toAlignedRect();

    int startTx = std::max(0, targetRect.left() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int endTx = std::min(m_tileManager.tilesX() - 1, targetRect.right() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int startTy = std::max(0, targetRect.top() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));
    int endTy = std::min(m_tileManager.tilesY() - 1, targetRect.bottom() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));

    for (int ty = startTy; ty <= endTy; ++ty) {
        for (int tx = startTx; tx <= endTx; ++tx) {
            QRect tileRect(tx * TileMemoryPool::TILE_WIDTH, ty * TileMemoryPool::TILE_HEIGHT,
                           TileMemoryPool::TILE_WIDTH, TileMemoryPool::TILE_HEIGHT);

            if (!tileRect.intersects(polyBounding)) {
                // タイル全体がポリゴン外接矩形の外側 -> 完全クリア
                Tile* t = m_tileManager.getTileForWrite(tx, ty);
                if (t && t->writeBuffer()) {
                    std::memset(t->writeBuffer(), 0, TileMemoryPool::TILE_BYTE_SIZE);
                }
                continue;
            }

            bool p0In = panelPath.contains(tileRect.topLeft());
            bool p1In = panelPath.contains(tileRect.topRight());
            bool p2In = panelPath.contains(tileRect.bottomLeft());
            bool p3In = panelPath.contains(tileRect.bottomRight());

            if (p0In && p1In && p2In && p3In) {
                // タイル全体が枠内 -> マスクの必要なし
            } else {
                Tile* t = m_tileManager.getTileForWrite(tx, ty);
                if (t && t->writeBuffer()) {
                    QImage tileImg(t->writeBuffer(), TileMemoryPool::TILE_WIDTH, TileMemoryPool::TILE_HEIGHT,
                                   TileMemoryPool::TILE_WIDTH * 4, QImage::Format_ARGB32_Premultiplied);
                    {
                        QPainter p(&tileImg);
                        p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
                        p.setRenderHint(QPainter::Antialiasing, true);
                        p.translate(-tileRect.x(), -tileRect.y());
                        p.fillPath(panelPath, Qt::black);
                    }
                }
            }

            // コマ枠線の描画
            if (m_drawPanelBorder && m_panelBorderWidth > 0.0f && m_panelBorderColor.alpha() > 0) {
                int pad = static_cast<int>(std::ceil(m_panelBorderWidth)) + 2;
                QRect borderBounds = polyBounding.adjusted(-pad, -pad, pad, pad);
                if (tileRect.intersects(borderBounds)) {
                    Tile* t = m_tileManager.getTileForWrite(tx, ty);
                    if (t && t->writeBuffer()) {
                        QImage tileImg(t->writeBuffer(), TileMemoryPool::TILE_WIDTH, TileMemoryPool::TILE_HEIGHT,
                                       TileMemoryPool::TILE_WIDTH * 4, QImage::Format_ARGB32_Premultiplied);
                        {
                            QPainter p(&tileImg);
                            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
                            p.setRenderHint(QPainter::Antialiasing, true);
                            p.translate(-tileRect.x(), -tileRect.y());
                            QPen pen(m_panelBorderColor, m_panelBorderWidth, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
                            p.strokePath(panelPath, pen);
                        }
                    }
                }
            }
        }
    }
}

} // namespace Kaburagi
