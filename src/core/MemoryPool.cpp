#include "MemoryPool.hpp"
#include <cstdlib>
#include <cstring>
#include <new>

namespace Kaburagi {

TileMemoryPool& TileMemoryPool::instance() {
    static TileMemoryPool s_instance;
    return s_instance;
}

TileMemoryPool::TileMemoryPool(size_t initialCapacity) {
    expandPool(initialCapacity);
}

TileMemoryPool::~TileMemoryPool() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (uint8_t* chunk : m_chunks) {
#if defined(_WIN32)
        _aligned_free(chunk);
#else
        std::free(chunk);
#endif
    }
    m_chunks.clear();
    m_freeList.clear();
}

void TileMemoryPool::expandPool(size_t additionalBlocks) {
    if (additionalBlocks == 0) return;

    size_t totalBytes = additionalBlocks * TILE_BYTE_SIZE;
#if defined(_WIN32)
    // 64バイトアライメント（AVX-512 / AVX2 に最適化）
    uint8_t* chunk = static_cast<uint8_t*>(_aligned_malloc(totalBytes, 64));
#else
    void* ptr = nullptr;
    if (posix_memalign(&ptr, 64, totalBytes) != 0) {
        throw std::bad_alloc();
    }
    uint8_t* chunk = static_cast<uint8_t*>(ptr);
#endif

    if (!chunk) {
        throw std::bad_alloc();
    }

    m_chunks.push_back(chunk);
    m_freeList.reserve(m_freeList.size() + additionalBlocks);

    for (size_t i = 0; i < additionalBlocks; ++i) {
        m_freeList.push_back(chunk + i * TILE_BYTE_SIZE);
    }
    m_totalCapacity += additionalBlocks;
}

uint8_t* TileMemoryPool::allocateTile() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_freeList.empty()) {
        expandPool(CHUNK_SIZE_BLOCKS);
    }
    uint8_t* ptr = m_freeList.back();
    m_freeList.pop_back();
    m_allocatedCount++;
    return ptr;
}

void TileMemoryPool::freeTile(uint8_t* ptr) {
    if (!ptr) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_freeList.push_back(ptr);
    if (m_allocatedCount > 0) {
        m_allocatedCount--;
    }
}

size_t TileMemoryPool::allocatedCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_allocatedCount;
}

size_t TileMemoryPool::capacity() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalCapacity;
}

} // namespace Kaburagi
