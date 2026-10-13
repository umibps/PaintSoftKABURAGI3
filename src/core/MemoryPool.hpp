#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <mutex>

namespace Kaburagi {

/**
 * @brief 固定長ブロック（タイル）用メモリスラブアロケータ
 * 描画ループ中での malloc/free を完全に排除し、O(1) でタイルメモリを貸出・返却する。
 */
class TileMemoryPool {
public:
    static constexpr size_t TILE_WIDTH = 128;
    static constexpr size_t TILE_HEIGHT = 128;
    static constexpr size_t BYTES_PER_PIXEL = 4; // RGBA8
    static constexpr size_t TILE_BYTE_SIZE = TILE_WIDTH * TILE_HEIGHT * BYTES_PER_PIXEL; // 64 KB

    // シングルトンまたはインスタンス
    static TileMemoryPool& instance();

    TileMemoryPool(size_t initialCapacity = 2048); // 初期2048タイル = 128MB
    ~TileMemoryPool();

    // コピー・代入禁止
    TileMemoryPool(const TileMemoryPool&) = delete;
    TileMemoryPool& operator=(const TileMemoryPool&) = delete;

    /**
     * @brief タイル用メモリブロックを取得 (mallocゼロ)
     * @return 64KBの連続したバッファへのポインタ
     */
    uint8_t* allocateTile();

    /**
     * @brief タイル用メモリブロックをプールに返却
     * @param ptr 貸出されたポインタ
     */
    void freeTile(uint8_t* ptr);

    size_t allocatedCount() const;
    size_t capacity() const;

private:
    void expandPool(size_t additionalBlocks);

    mutable std::mutex m_mutex;
    std::vector<uint8_t*> m_chunks;       // 確保された大容量チャンク
    std::vector<uint8_t*> m_freeList;     // 返却済み空きスロット
    size_t m_allocatedCount = 0;
    size_t m_totalCapacity = 0;
    static constexpr size_t CHUNK_SIZE_BLOCKS = 512; // 拡張単位 (32MB)
};

} // namespace Kaburagi
