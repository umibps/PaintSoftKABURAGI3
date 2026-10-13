#pragma once

#include "Tile.hpp"
#include <vector>
#include <unordered_set>
#include <QRect>
#include <QImage>

namespace Kaburagi {

/**
 * @brief 2次元タイルグリッド管理クラス
 * キャンバス上の矩形領域を 128x128 のタイル群で管理。
 * 未描画タイルは nullptr (ゼロアロケーション) で保持し、書き込み時のみプールから割り当てる。
 */
class TileManager {
public:
    TileManager(int width = 0, int height = 0);
    ~TileManager() = default;

    int width() const { return m_width; }
    int height() const { return m_height; }
    int tilesX() const { return m_tilesX; }
    int tilesY() const { return m_tilesY; }

    // タイル取得 (読み取り)
    const Tile* getTile(int tx, int ty) const;

    // タイル取得 (書き込み用・必要に応じてメモリ割当)
    Tile* getTileForWrite(int tx, int ty);

    // ピクセル操作 (直接)
    void setPixel(int x, int y, uint32_t argb);
    uint32_t getPixel(int x, int y) const;

    // ダーティ領域管理
    void markDirtyRect(const QRect& rect);
    void clearDirty();
    const std::vector<int>& dirtyTileIndices() const { return m_dirtyIndices; }
    QRect dirtyBoundingRect() const { return m_dirtyBoundingRect; }

    // キャンバス全体のクリア
    void clear(uint32_t fillColor = 0x00000000);

    // スナップショット作成 (Copy-on-Write による高速アンドゥ用)
    TileManager clone() const;

    // 平行移動 (オフセット移動)
    void translate(int dx, int dy);

    // 有効ピクセル（不透明ピクセル）が存在するバウンディング矩形を計算
    QRect calculateContentBounds() const;

    // QImage への変換および読み込み、サムネイル生成
    QImage toQImage() const;
    void fromQImage(const QImage& img);
    QImage createThumbnail(int maxW = 256, int maxH = 256) const;

private:
    int m_width;
    int m_height;
    int m_tilesX;
    int m_tilesY;

    std::vector<Tile> m_tiles;
    std::vector<int> m_dirtyIndices;
    QRect m_dirtyBoundingRect;
};

} // namespace Kaburagi
