#include <hex/providers/memory_provider.hpp>

#include <cstring>

namespace hex::prv {

    bool MemoryProvider::open() {
        if (m_data.empty()) {
            m_data.resize(1);
        }

        return true;
    }

    void MemoryProvider::readRaw(u64 offset, void *buffer, size_t size) {
        auto actualSize = this->getActualSize();
        if (actualSize == 0 || (offset + size) > actualSize || buffer == nullptr || size == 0)
            return;

        std::memcpy(buffer, &m_data.front() + offset, size);
    }

    void MemoryProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        if ((offset + size) > this->getActualSize() || buffer == nullptr || size == 0)
            return;

        std::memcpy(&m_data.front() + offset, buffer, size);
    }

    void MemoryProvider::resizeRaw(u64 newSize) {
        m_data.resize(newSize);
    }

    void MemoryProvider::insertRaw(u64 offset, u64 size) {
        auto oldSize = this->getActualSize();
        this->resizeRaw(oldSize + size);

        std::vector<u8> buffer(0x1000);
        const std::vector<u8> zeroBuffer(0x1000);

        auto position = oldSize;
        while (position > offset) {
            const auto readSize = std::min<size_t>(position - offset, buffer.size());

            position -= readSize;

            this->readRaw(position, buffer.data(), readSize);
            this->writeRaw(position, zeroBuffer.data(), readSize);
            this->writeRaw(position + size, buffer.data(), readSize);
        }
    }

    void MemoryProvider::removeRaw(u64 offset, u64 size) {
        auto oldSize = this->getActualSize();
        std::vector<u8> buffer(0x1000);

        const auto newSize = oldSize - size;
        auto position = offset;
        while (position < newSize) {
            const auto readSize = std::min<size_t>(newSize - position, buffer.size());

            this->readRaw(position + size, buffer.data(), readSize);
            this->writeRaw(position, buffer.data(), readSize);

            position += readSize;
        }

        this->resizeRaw(oldSize - size);
    }

}
