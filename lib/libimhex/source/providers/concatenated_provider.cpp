#include <hex/providers/concatenated_provider.hpp>

#include <algorithm>
#include <cstring>
#include <limits>

namespace hex::prv {

    ConcatenatedProvider::ConcatenatedProvider(std::vector<Segment> segments) {
        for (const auto &[provider, region] : segments)
            this->add(provider, region);
    }

    ConcatenatedProvider::ConcatenatedProvider(std::initializer_list<Segment> segments) {
        for (const auto &[provider, region] : segments)
            this->add(provider, region);
    }

    void ConcatenatedProvider::add(Provider *provider, const Region &region) {
        if (provider == nullptr || region.size == 0 || region.size > std::numeric_limits<u64>::max() - m_size)
            return;

        m_segments.push_back({ provider, region });
        m_size += region.size;
    }

    void ConcatenatedProvider::add(Provider *provider) {
        if (provider != nullptr)
            this->add(provider, { provider->getBaseAddress(), provider->getActualSize() });
    }

    bool ConcatenatedProvider::isAvailable() const {
        return std::ranges::all_of(m_segments, [](const auto &segment) {
            return segment.provider->isAvailable();
        });
    }

    bool ConcatenatedProvider::isReadable() const {
        return std::ranges::all_of(m_segments, [](const auto &segment) {
            return segment.provider->isReadable();
        });
    }

    void ConcatenatedProvider::readRaw(u64 offset, void *buffer, size_t size) {
        if (buffer == nullptr || size == 0 || offset > m_size || size > m_size - offset)
            return;

        auto output = static_cast<u8*>(buffer);
        for (const auto &[provider, region] : m_segments) {
            if (offset >= region.size) {
                offset -= region.size;
                continue;
            }

            const auto readSize = std::min<u64>(size, region.size - offset);
            provider->read(region.address + offset, output, static_cast<size_t>(readSize));

            output += readSize;
            size -= static_cast<size_t>(readSize);
            offset = 0;

            if (size == 0)
                break;
        }
    }

}
