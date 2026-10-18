#include "TileManager.hpp"
#include <algorithm>

namespace Kaburagi {

TileManager::TileManager(int width, int height)
    : m_width(std::max(0, width)), m_height(std::max(0, height)) {
    if (m_width > 0 && m_height > 0) {
        m_tilesX = (m_width + TileMemoryPool::TILE_WIDTH - 1) / TileMemoryPool::TILE_WIDTH;
        m_tilesY = (m_height + TileMemoryPool::TILE_HEIGHT - 1) / TileMemoryPool::TILE_HEIGHT;
        m_tiles.resize(m_tilesX * m_tilesY);
    } else {
        m_tilesX = 0;
        m_tilesY = 0;
    }
}

const Tile* TileManager::getTile(int tx, int ty) const {
    if (tx < 0 || tx >= m_tilesX || ty < 0 || ty >= m_tilesY) {
        return nullptr;
    }
    const Tile& t = m_tiles[ty * m_tilesX + tx];
    return t.isValid() ? &t : nullptr;
}

Tile* TileManager::getTileForWrite(int tx, int ty) {
    if (tx < 0 || tx >= m_tilesX || ty < 0 || ty >= m_tilesY) {
        return nullptr;
    }
    int idx = ty * m_tilesX + tx;
    // タイルがまだ割り当てられていない場合は確保
    if (!m_tiles[idx].isValid()) {
        m_tiles[idx].writeBuffer(); // 自動確保
    }
    return &m_tiles[idx];
}

void TileManager::setPixel(int x, int y, uint32_t argb) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;

    int tx = x / TileMemoryPool::TILE_WIDTH;
    int ty = y / TileMemoryPool::TILE_HEIGHT;
    int lx = x % TileMemoryPool::TILE_WIDTH;
    int ly = y % TileMemoryPool::TILE_HEIGHT;

    Tile* tile = getTileForWrite(tx, ty);
    if (!tile) return;

    uint32_t* ptr = reinterpret_cast<uint32_t*>(tile->writeBuffer());
    ptr[ly * TileMemoryPool::TILE_WIDTH + lx] = argb;

    markDirtyRect(QRect(x, y, 1, 1));
}

uint32_t TileManager::getPixel(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return 0;

    int tx = x / TileMemoryPool::TILE_WIDTH;
    int ty = y / TileMemoryPool::TILE_HEIGHT;
    int lx = x % TileMemoryPool::TILE_WIDTH;
    int ly = y % TileMemoryPool::TILE_HEIGHT;

    const Tile* tile = getTile(tx, ty);
    if (!tile || !tile->readBuffer()) return 0;

    const uint32_t* ptr = reinterpret_cast<const uint32_t*>(tile->readBuffer());
    return ptr[ly * TileMemoryPool::TILE_WIDTH + lx];
}

void TileManager::markDirtyRect(const QRect& rect) {
    if (rect.isEmpty()) return;

    m_dirtyBoundingRect = m_dirtyBoundingRect.isNull() ? rect : m_dirtyBoundingRect.united(rect);

    int startTx = std::max(0, rect.left() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int endTx = std::min(m_tilesX - 1, rect.right() / static_cast<int>(TileMemoryPool::TILE_WIDTH));
    int startTy = std::max(0, rect.top() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));
    int endTy = std::min(m_tilesY - 1, rect.bottom() / static_cast<int>(TileMemoryPool::TILE_HEIGHT));

    for (int ty = startTy; ty <= endTy; ++ty) {
        for (int tx = startTx; tx <= endTx; ++tx) {
            int idx = ty * m_tilesX + tx;
            if (std::find(m_dirtyIndices.begin(), m_dirtyIndices.end(), idx) == m_dirtyIndices.end()) {
                m_dirtyIndices.push_back(idx);
            }
        }
    }
}

void TileManager::clearDirty() {
    m_dirtyIndices.clear();
    m_dirtyBoundingRect = QRect();
}

void TileManager::clear(uint32_t fillColor) {
    if (fillColor == 0) {
        // 完全に透明にする場合は全タイルを解放するだけで即座にメモリゼロ化
        for (Tile& t : m_tiles) {
            t.reset();
        }
    } else {
        for (int idx = 0; idx < m_tilesX * m_tilesY; ++idx) {
            uint32_t* ptr = reinterpret_cast<uint32_t*>(m_tiles[idx].writeBuffer());
            for (size_t i = 0; i < TileMemoryPool::TILE_WIDTH * TileMemoryPool::TILE_HEIGHT; ++i) {
                ptr[i] = fillColor;
            }
        }
    }
    markDirtyRect(QRect(0, 0, m_width, m_height));
}

TileManager TileManager::clone() const {
    TileManager copy(m_width, m_height);
    // CoW: Tile のコピーコンストラクタによりポインタと参照カウントが引き継がれ、実コピーは発生しない
    copy.m_tiles = m_tiles;
    return copy;
}

void TileManager::translate(int dx, int dy) {
    if (dx == 0 && dy == 0) return;

    TileManager oldTiles = clone();
    clear(0x00000000);

    for (int ty = 0; ty < m_tilesY; ++ty) {
        for (int tx = 0; tx < m_tilesX; ++tx) {
            const Tile* srcT = oldTiles.getTile(tx, ty);
            if (!srcT || !srcT->readBuffer()) continue;

            const uint32_t* srcBuf = reinterpret_cast<const uint32_t*>(srcT->readBuffer());
            int srcStartX = tx * 128;
            int srcStartY = ty * 128;

            for (int ly = 0; ly < 128; ++ly) {
                int sy = srcStartY + ly;
                int dyPos = sy + dy;
                if (dyPos < 0 || dyPos >= m_height) continue;

                for (int lx = 0; lx < 128; ++lx) {
                    uint32_t p = srcBuf[ly * 128 + lx];
                    if (p == 0) continue;

                    int sx = srcStartX + lx;
                    int dxPos = sx + dx;
                    if (dxPos < 0 || dxPos >= m_width) continue;

                    setPixel(dxPos, dyPos, p);
                }
            }
        }
    }
    markDirtyRect(QRect(0, 0, m_width, m_height));
}

QRect TileManager::calculateContentBounds() const {
    int minX = m_width;
    int minY = m_height;
    int maxX = -1;
    int maxY = -1;

    for (int ty = 0; ty < m_tilesY; ++ty) {
        for (int tx = 0; tx < m_tilesX; ++tx) {
            int tileIdx = ty * m_tilesX + tx;
            const Tile& tile = m_tiles[tileIdx];
            if (!tile.isValid() || !tile.readBuffer()) continue;

            const uint32_t* pixels = reinterpret_cast<const uint32_t*>(tile.readBuffer());
            int baseDocX = tx * TileMemoryPool::TILE_WIDTH;
            int baseDocY = ty * TileMemoryPool::TILE_HEIGHT;

            for (int ly = 0; ly < static_cast<int>(TileMemoryPool::TILE_HEIGHT); ++ly) {
                int docY = baseDocY + ly;
                if (docY >= m_height) break;

                const uint32_t* row = pixels + (ly * TileMemoryPool::TILE_WIDTH);
                for (int lx = 0; lx < static_cast<int>(TileMemoryPool::TILE_WIDTH); ++lx) {
                    int docX = baseDocX + lx;
                    if (docX >= m_width) break;

                    uint32_t p = row[lx];
                    if ((p >> 24) > 0) { // アルファ > 0
                        if (docX < minX) minX = docX;
                        if (docX > maxX) maxX = docX;
                        if (docY < minY) minY = docY;
                        if (docY > maxY) maxY = docY;
                    }
                }
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        return QRect(); // 有効な描画ピクセルなし
    }

    return QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

QImage TileManager::toQImage() const {
    if (m_width <= 0 || m_height <= 0) {
        return QImage();
    }
    QImage img(m_width, m_height, QImage::Format_ARGB32);
    img.fill(0);

    for (int ty = 0; ty < m_tilesY; ++ty) {
        for (int tx = 0; tx < m_tilesX; ++tx) {
            const Tile* tile = getTile(tx, ty);
            if (!tile || !tile->isValid()) continue;

            const uint32_t* src = reinterpret_cast<const uint32_t*>(tile->readBuffer());
            int startX = tx * TileMemoryPool::TILE_WIDTH;
            int startY = ty * TileMemoryPool::TILE_HEIGHT;
            int copyW = std::min(static_cast<int>(TileMemoryPool::TILE_WIDTH), m_width - startX);
            int copyH = std::min(static_cast<int>(TileMemoryPool::TILE_HEIGHT), m_height - startY);

            for (int y = 0; y < copyH; ++y) {
                uint32_t* dst = reinterpret_cast<uint32_t*>(img.scanLine(startY + y)) + startX;
                std::memcpy(dst, src + y * TileMemoryPool::TILE_WIDTH, copyW * sizeof(uint32_t));
            }
        }
    }
    return img;
}

void TileManager::fromQImage(const QImage& img) {
    if (img.isNull() || m_width <= 0 || m_height <= 0) return;
    QImage srcImg = img;
    if (srcImg.format() != QImage::Format_ARGB32) {
        srcImg = srcImg.convertToFormat(QImage::Format_ARGB32);
    }
    int copyW = std::min(m_width, srcImg.width());
    int copyH = std::min(m_height, srcImg.height());

    for (int ty = 0; ty < m_tilesY; ++ty) {
        int startY = ty * TileMemoryPool::TILE_HEIGHT;
        if (startY >= copyH) break;
        int tileH = std::min(static_cast<int>(TileMemoryPool::TILE_HEIGHT), copyH - startY);

        for (int tx = 0; tx < m_tilesX; ++tx) {
            int startX = tx * TileMemoryPool::TILE_WIDTH;
            if (startX >= copyW) break;
            int tileW = std::min(static_cast<int>(TileMemoryPool::TILE_WIDTH), copyW - startX);

            Tile* tile = getTileForWrite(tx, ty);
            if (!tile) continue;
            uint32_t* dst = reinterpret_cast<uint32_t*>(tile->writeBuffer());

            for (int y = 0; y < tileH; ++y) {
                const uint32_t* src = reinterpret_cast<const uint32_t*>(srcImg.constScanLine(startY + y)) + startX;
                std::memcpy(dst + y * TileMemoryPool::TILE_WIDTH, src, tileW * sizeof(uint32_t));
            }
        }
    }
    markDirtyRect(QRect(0, 0, copyW, copyH));
}

QImage TileManager::createThumbnail(int maxW, int maxH) const {
    if (m_width <= 0 || m_height <= 0) {
        return QImage();
    }
    double scale = std::min(static_cast<double>(maxW) / m_width, static_cast<double>(maxH) / m_height);
    if (scale >= 1.0) {
        return toQImage();
    }

    int tw = std::max(1, static_cast<int>(m_width * scale));
    int th = std::max(1, static_cast<int>(m_height * scale));
    QImage thumb(tw, th, QImage::Format_ARGB32);
    thumb.fill(0);

    int lastTx = -1, lastTy = -1;
    const uint32_t* tilePixels = nullptr;

    for (int y = 0; y < th; ++y) {
        int docY = (y * m_height) / th;
        int ty = docY / TileMemoryPool::TILE_HEIGHT;
        int ly = docY % TileMemoryPool::TILE_HEIGHT;
        uint32_t* line = reinterpret_cast<uint32_t*>(thumb.scanLine(y));

        for (int x = 0; x < tw; ++x) {
            int docX = (x * m_width) / tw;
            int tx = docX / TileMemoryPool::TILE_WIDTH;
            int lx = docX % TileMemoryPool::TILE_WIDTH;

            if (tx != lastTx || ty != lastTy) {
                lastTx = tx;
                lastTy = ty;
                const Tile* tile = getTile(tx, ty);
                tilePixels = (tile && tile->isValid()) ? reinterpret_cast<const uint32_t*>(tile->readBuffer()) : nullptr;
            }

            if (tilePixels) {
                line[x] = tilePixels[ly * TileMemoryPool::TILE_WIDTH + lx];
            }
        }
    }
    return thumb;
}

} // namespace Kaburagi
