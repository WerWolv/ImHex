#include "hex/providers/cached_provider.hpp"
#include <algorithm>
#include <optional>

namespace hex::prv {

    CachedProvider::CachedProvider(size_t cacheBlockSize, size_t maxBlocks)
        : m_cacheBlockSize(cacheBlockSize), m_maxBlocks(maxBlocks), m_cache(maxBlocks) {}

    CachedProvider::~CachedProvider() {
        clearCache();
    }

    Provider::OpenResult CachedProvider::open() {
        clearCache();
        return {};
    }

    void CachedProvider::close() {
        clearCache();
    }

    void CachedProvider::readRaw(u64 offset, void* buffer, size_t size) {
        if (!isAvailable() || !isReadable())
            return;

        auto out = static_cast<u8 *>(buffer);
        while (size > 0) {
            const auto blockIndex = calcBlockIndex(offset);
            const auto blockOffset = calcBlockOffset(offset);
            const auto toRead = std::min(m_cacheBlockSize - blockOffset, size);
            const auto cacheSlot = blockIndex % m_maxBlocks;

            {
                std::shared_lock lock(m_cacheMutex);
                const auto &slot = m_cache[cacheSlot];
                if (slot && slot->index == blockIndex) {
                    std::copy_n(slot->data.begin() + blockOffset, toRead, out);

                    out += toRead;
                    offset += toRead;
                    size -= toRead;
                    continue;
                }
            }

            std::vector<uint8_t> blockData(m_cacheBlockSize);
            readFromSource(blockIndex * m_cacheBlockSize, blockData.data(), m_cacheBlockSize);

            {
                std::unique_lock lock(m_cacheMutex);
                m_cache[cacheSlot] = Block{.index=blockIndex, .data=std::move(blockData), .dirty=false};
                std::copy_n(m_cache[cacheSlot]->data.begin() + blockOffset, toRead, out);
            }

            out += toRead;
            offset += toRead;
            size -= toRead;
        }
    }

    void CachedProvider::writeRaw(u64 offset, const void* buffer, size_t size) {
        if (!isAvailable() || !isWritable())
            return;

        auto in = static_cast<const u8 *>(buffer);
        while (size > 0) {
            const auto blockIndex = calcBlockIndex(offset);
            const auto blockOffset = calcBlockOffset(offset);
            const auto toWrite = std::min(m_cacheBlockSize - blockOffset, size);
            const auto cacheSlot = blockIndex % m_maxBlocks;

            {
                std::unique_lock lock(m_cacheMutex);
                auto& slot = m_cache[cacheSlot];
                if (!slot || slot->index != blockIndex) {
                    std::vector<uint8_t> blockData(m_cacheBlockSize);
                    readFromSource(blockIndex * m_cacheBlockSize, blockData.data(), m_cacheBlockSize);
                    slot = Block { .index=blockIndex, .data=std::move(blockData), .dirty=false };
                }

                std::copy_n(in, toWrite, slot->data.begin() + blockOffset);
                slot->dirty = true;
            }

            writeToSource(offset, in, toWrite);

            in += toWrite;
            offset += toWrite;
            size -= toWrite;
        }
    }

    void CachedProvider::resizeRaw(u64 newSize) {
        clearCache();

        resizeSource(newSize);
    }


    u64 CachedProvider::getActualSize() const {
        if (!isAvailable())
            return 0;

        if (m_cachedSize == 0) {
            std::unique_lock lock(m_cacheMutex);
            m_cachedSize = getSourceSize();
        }

        return m_cachedSize;
    }


    void CachedProvider::clearCache() {
        std::unique_lock lock(m_cacheMutex);

        for (auto& slot : m_cache)
            slot.reset();

        m_cachedSize = 0;
    }

    void CachedProvider::evictIfNeeded() {
        if (m_cache.size() < m_maxBlocks)
            return;

        m_cache.erase(m_cache.begin());
    }

}
