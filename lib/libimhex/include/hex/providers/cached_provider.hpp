#pragma once

#include <hex/providers/provider.hpp>

#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <cstddef>
#include <cstdint>

namespace hex::prv {

    /**
     * @brief A base class for providers that want to cache data in memory.
     *        Thread-safe for concurrent reads/writes. Reads are cached in memory.
     *        Subclasses must implement readFromSource and writeToSource.
     */
    class CachedProvider : public Provider {
    public:
        CachedProvider(size_t cacheBlockSize = 4096, size_t maxBlocks = 1024);
        ~CachedProvider() override;

        OpenResult open() override;
        void close() override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        void resizeRaw(u64 newSize) override;

        u64 getActualSize() const override;

    protected:
        virtual void readFromSource(uint64_t offset, void* buffer, size_t size) = 0;
        virtual void writeToSource(uint64_t offset, const void* buffer, size_t size) = 0;
        virtual void resizeSource(uint64_t newSize) { std::ignore = newSize; }
        virtual u64 getSourceSize() const = 0;

        void clearCache();

        struct Block {
            uint64_t index;
            std::vector<uint8_t> data;
            bool dirty = false;
        };

        size_t m_cacheBlockSize;
        size_t m_maxBlocks;
        mutable std::shared_mutex m_cacheMutex;
        std::vector<std::optional<Block>> m_cache;
        mutable u64 m_cachedSize = 0;

        constexpr u64 calcBlockIndex(u64 offset) const { return offset / m_cacheBlockSize; }
        constexpr size_t calcBlockOffset(u64 offset) const { return offset % m_cacheBlockSize; }

        void evictIfNeeded();
    };

}
