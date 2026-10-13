#pragma once

#include "MemoryPool.hpp"
#include <atomic>
#include <cstring>

namespace Kaburagi {

/**
 * @brief タイルデータクラス (参照カウント付き Copy-on-Write)
 */
class TileData {
public:
    TileData() {
        m_buffer = TileMemoryPool::instance().allocateTile();
        std::memset(m_buffer, 0, TileMemoryPool::TILE_BYTE_SIZE);
        m_refCount.store(1, std::memory_order_relaxed);
    }

    ~TileData() {
        if (m_buffer) {
            TileMemoryPool::instance().freeTile(m_buffer);
        }
    }

    void retain() {
        m_refCount.fetch_add(1, std::memory_order_relaxed);
    }

    bool release() {
        if (m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
            return true;
        }
        return false;
    }

    uint8_t* buffer() { return m_buffer; }
    const uint8_t* buffer() const { return m_buffer; }

    int refCount() const { return m_refCount.load(std::memory_order_relaxed); }

    TileData* clone() const {
        TileData* copy = new TileData();
        std::memcpy(copy->m_buffer, m_buffer, TileMemoryPool::TILE_BYTE_SIZE);
        return copy;
    }

private:
    uint8_t* m_buffer = nullptr;
    std::atomic<int> m_refCount{1};
};

/**
 * @brief スマートポインタ風タイルラッパー
 */
class Tile {
public:
    Tile() : m_data(nullptr) {}
    explicit Tile(TileData* data) : m_data(data) {}

    Tile(const Tile& other) : m_data(other.m_data) {
        if (m_data) m_data->retain();
    }

    Tile& operator=(const Tile& other) {
        if (this != &other) {
            if (m_data) m_data->release();
            m_data = other.m_data;
            if (m_data) m_data->retain();
        }
        return *this;
    }

    Tile(Tile&& other) noexcept : m_data(other.m_data) {
        other.m_data = nullptr;
    }

    Tile& operator=(Tile&& other) noexcept {
        if (this != &other) {
            if (m_data) m_data->release();
            m_data = other.m_data;
            other.m_data = nullptr;
        }
        return *this;
    }

    ~Tile() {
        if (m_data) {
            m_data->release();
        }
    }

    bool isValid() const { return m_data != nullptr; }

    // 読み込み専用アクセス
    const uint8_t* readBuffer() const {
        return m_data ? m_data->buffer() : nullptr;
    }

    // 書き込み用アクセス (CoW: 共有されていれば複製)
    uint8_t* writeBuffer() {
        if (!m_data) {
            m_data = new TileData();
            return m_data->buffer();
        }
        if (m_data->refCount() > 1) {
            TileData* cloned = m_data->clone();
            m_data->release();
            m_data = cloned;
        }
        return m_data->buffer();
    }

    void reset() {
        if (m_data) {
            m_data->release();
            m_data = nullptr;
        }
    }

private:
    TileData* m_data = nullptr;
};

} // namespace Kaburagi
