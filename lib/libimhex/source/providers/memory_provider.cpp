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

}
