#include "Layer.hpp"
#include "GroupLayer.hpp"
#include <QPainter>
#include <algorithm>

namespace Kaburagi {

Layer::Layer(const QString& name, int width, int height, LayerType type)
    : m_name(name), m_type(type), m_tileManager(width, height) {
}

std::shared_ptr<GroupLayer> Layer::parentGroup() const {
    return m_parentGroup.lock();
}

void Layer::setParentGroup(const std::shared_ptr<GroupLayer>& group) {
    m_parentGroup = group;
}

bool Layer::effectiveVisible() const {
    if (!m_visible) return false;
    auto parent = parentGroup();
    if (parent) {
        return parent->effectiveVisible();
    }
    return true;
}

float Layer::effectiveOpacity() const {
    float op = m_opacity;
    auto parent = parentGroup();
    if (parent) {
        op *= parent->effectiveOpacity();
    }
    return op;
}

void Layer::renderToTarget(TileManager& target, const QRect& dirtyRect, const Layer* baseLayer, bool clipToHiddenBase) {
    if (!m_visible || m_opacity <= 0.0f) return;

    // クリッピングマスクが有効な場合
    if (m_isClippingMask) {
        if (!baseLayer || (!clipToHiddenBase && !baseLayer->isVisible()) || baseLayer->effectiveOpacity() <= 0.0f) {
            // ベースレイヤーが存在しない、非表示クリッピング無効時にベース非表示、または不透明度が0なら描画しない
            return;
        }
    }

    int startTx = std::max(0, dirtyRect.left() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int endTx = std::min(m_tileManager.tilesX() - 1, dirtyRect.right() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int startTy = std::max(0, dirtyRect.top() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));
    int endTy = std::min(m_tileManager.tilesY() - 1, dirtyRect.bottom() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));

    constexpr size_t PIXELS_PER_TILE = TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT;

    for (int ty = startTy; ty <= endTy; ++ty) {
        for (int tx = startTx; tx <= endTx; ++tx) {
            const Tile* srcTile = m_tileManager.getTile(tx, ty);
            if (!srcTile || !srcTile->readBuffer()) {
                // このレイヤーのタイルが空なら合成不要
                continue;
            }

            const Tile* maskTile = nullptr;
            if (m_isClippingMask && baseLayer) {
                maskTile = baseLayer->tileManager().getTile(tx, ty);
                if (!maskTile || !maskTile->readBuffer()) {
                    // マスクタイルのピクセルがすべて透明なら描画不要
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

QRect Layer::contentBounds() const {
    if (!m_contentBoundsValid) {
        m_cachedContentBounds = m_tileManager.calculateContentBounds();
        m_contentBoundsValid = true;
    }
    return m_cachedContentBounds;
}

void Layer::translate(int dx, int dy) {
    m_tileManager.translate(dx, dy);
    if (m_contentBoundsValid && !m_cachedContentBounds.isEmpty()) {
        m_cachedContentBounds.translate(dx, dy);
    }
}

QImage Layer::createThumbnail(int thumbW, int thumbH) const {
    if (thumbW <= 0 || thumbH <= 0) {
        return QImage();
    }

    QImage thumb(thumbW, thumbH, QImage::Format_ARGB32_Premultiplied);
    thumb.fill(QColor(230, 230, 230)); // 外側の余白は薄いグレー

    int docW = m_tileManager.width();
    int docH = m_tileManager.height();
    if (docW <= 0 || docH <= 0) {
        return thumb;
    }

    // レイヤー使用範囲（バウンディング矩形）の取得
    QRect bounds = contentBounds();
    QRect cropRect;
    if (!bounds.isEmpty()) {
        // 使用部分の周囲に少し余白（マージン）を付与
        int padX = std::max(2, bounds.width() / 16);
        int padY = std::max(2, bounds.height() / 16);
        cropRect = bounds.adjusted(-padX, -padY, padX, padY).intersected(QRect(0, 0, docW, docH));
    } else {
        // 描画が存在しない場合はキャンバス全体を表示
        cropRect = QRect(0, 0, docW, docH);
    }

    int cropW = std::max(1, cropRect.width());
    int cropH = std::max(1, cropRect.height());

    // cropRectのアスペクト比を維持した描画矩形を計算 (内側 1px マージン)
    double scale = std::min(static_cast<double>(thumbW - 2) / cropW, static_cast<double>(thumbH - 2) / cropH);
    int rw = std::clamp(static_cast<int>(cropW * scale + 0.5), 4, thumbW - 2);
    int rh = std::clamp(static_cast<int>(cropH * scale + 0.5), 4, thumbH - 2);
    int rx = (thumbW - rw) / 2;
    int ry = (thumbH - rh) / 2;

    // 市松模様（チェッカーボード）をサムネイル枠内に描画
    int checkSize = 3;
    for (int y = ry; y < ry + rh; ++y) {
        uint32_t* line = reinterpret_cast<uint32_t*>(thumb.scanLine(y));
        for (int x = rx; x < rx + rw; ++x) {
            bool isDark = (((x - rx) / checkSize) + ((y - ry) / checkSize)) % 2 == 1;
            line[x] = isDark ? 0xFFD8D8D8 : 0xFFFFFFFF;
        }
    }

    if (!bounds.isEmpty()) {
        // 切り抜いた使用範囲から高速サンプリング (最大でも 36x28 = 約1000ルックアップ)
        int lastTx = -1, lastTy = -1;
        const uint32_t* tilePixels = nullptr;

        for (int y = 0; y < rh; ++y) {
            int docY = cropRect.top() + (y * cropH) / rh;
            int ty = docY / TileMemoryPool::TILE_HEIGHT;
            int ly = docY % TileMemoryPool::TILE_HEIGHT;
            uint32_t* line = reinterpret_cast<uint32_t*>(thumb.scanLine(ry + y));

            for (int x = 0; x < rw; ++x) {
                int docX = cropRect.left() + (x * cropW) / rw;
                int tx = docX / TileMemoryPool::TILE_WIDTH;
                int lx = docX % TileMemoryPool::TILE_WIDTH;

                if (tx != lastTx || ty != lastTy) {
                    lastTx = tx;
                    lastTy = ty;
                    const Tile* tile = m_tileManager.getTile(tx, ty);
                    tilePixels = (tile && tile->isValid()) ? reinterpret_cast<const uint32_t*>(tile->readBuffer()) : nullptr;
                }

                if (!tilePixels) continue;

                uint32_t p = tilePixels[ly * TileMemoryPool::TILE_WIDTH + lx];
                uint32_t a = (p >> 24) & 0xFF;
                if (a == 0) continue;

                uint32_t r = (p >> 16) & 0xFF;
                uint32_t g = (p >> 8) & 0xFF;
                uint32_t b = p & 0xFF;

                int dstX = rx + x;
                if (a == 255) {
                    line[dstX] = 0xFF000000 | (r << 16) | (g << 8) | b;
                } else {
                    // チェッカー背景と Over ブレンド
                    uint32_t bg = line[dstX];
                    uint32_t bgR = (bg >> 16) & 0xFF;
                    uint32_t bgG = (bg >> 8) & 0xFF;
                    uint32_t bgB = bg & 0xFF;

                    float alpha = a / 255.0f;
                    uint32_t outR = static_cast<uint32_t>(r * alpha + bgR * (1.0f - alpha) + 0.5f);
                    uint32_t outG = static_cast<uint32_t>(g * alpha + bgG * (1.0f - alpha) + 0.5f);
                    uint32_t outB = static_cast<uint32_t>(b * alpha + bgB * (1.0f - alpha) + 0.5f);
                    line[dstX] = 0xFF000000 | (outR << 16) | (outG << 8) | outB;
                }
            }
        }
    }

    // キャンバス外枠線を描画
    QPainter p(&thumb);
    p.setPen(QColor(140, 140, 140));
    p.drawRect(rx - 1, ry - 1, rw + 1, rh + 1);

    return thumb;
}

} // namespace Kaburagi
