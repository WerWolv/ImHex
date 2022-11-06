#include "content/providers/memory_file_provider.hpp"
#include "content/providers/file_provider.hpp"

#include <cstring>

#include <hex/api/imhex_api.hpp>
#include <hex/api/localization.hpp>
#include <hex/api/event.hpp>
#include <hex/helpers/file.hpp>

namespace hex::plugin::builtin {

    bool MemoryFileProvider::open() {
        this->m_data.resize(1);
        this->markDirty();
        return true;
    }

    void MemoryFileProvider::readRaw(u64 offset, void *buffer, size_t size) {
        if ((offset + size) > this->getActualSize() || buffer == nullptr || size == 0)
            return;

        std::memcpy(buffer, &this->m_data.front() + offset, size);
    }

    void MemoryFileProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        if ((offset + size) > this->getActualSize() || buffer == nullptr || size == 0)
            return;

        std::memcpy(&this->m_data.front() + offset, buffer, size);
    }

    void MemoryFileProvider::save() {
        fs::openFileBrowser(fs::DialogMode::Save, { }, [this](const std::fs::path &path) {
            if (path.empty())
                return;

            this->saveAs(path);

            auto newProvider = hex::ImHexApi::Provider::createProvider("hex.builtin.provider.file", true);

            if (auto fileProvider = dynamic_cast<FileProvider*>(newProvider); fileProvider != nullptr && (fileProvider->setPath(path), !fileProvider->open()))
                ImHexApi::Provider::remove(newProvider);
            else {
                fileProvider->markDirty(false);
                EventManager::post<EventProviderOpened>(newProvider);
                ImHexApi::Provider::remove(this, true);
            }
        });
    }

    void MemoryFileProvider::saveAs(const std::fs::path &path) {
        fs::File file(path, fs::File::Mode::Create);

        if (file.isValid()) {
            auto provider = ImHexApi::Provider::get();

            std::vector<u8> buffer(std::min<size_t>(0xFF'FFFF, provider->getActualSize()), 0x00);
            size_t bufferSize = buffer.size();

            for (u64 offset = 0; offset < provider->getActualSize(); offset += bufferSize) {
                if (bufferSize > provider->getActualSize() - offset)
                    bufferSize = provider->getActualSize() - offset;

                provider->read(offset + this->getBaseAddress(), buffer.data(), bufferSize);
                file.write(buffer);
            }
        }
    }

    void MemoryFileProvider::resize(size_t newSize) {
        this->m_data.resize(newSize);

        Provider::resize(newSize);
    }

    void MemoryFileProvider::insert(u64 offset, size_t size) {
        auto oldSize = this->getActualSize();
        this->resize(oldSize + size);

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

        Provider::insert(offset, size);
    }

    void MemoryFileProvider::remove(u64 offset, size_t size) {
        auto oldSize = this->getActualSize();
        this->resize(oldSize + size);

        std::vector<u8> buffer(0x1000);

        const auto newSize = oldSize - size;
        auto position = offset;
        while (position < newSize) {
            const auto readSize = std::min<size_t>(newSize - position, buffer.size());

            this->readRaw(position + size, buffer.data(), readSize);
            this->writeRaw(position, buffer.data(), readSize);

            position += readSize;
        }

        this->resize(newSize);

        Provider::insert(offset, size);
        Provider::remove(offset, size);
    }

    std::pair<Region, bool> MemoryFileProvider::getRegionValidity(u64 address) const {
        address -= this->getBaseAddress();

        if (address < this->getActualSize())
            return { Region { this->getBaseAddress() + address, this->getActualSize() - address }, true };
        else
            return { Region::Invalid(), false };
    }

}
