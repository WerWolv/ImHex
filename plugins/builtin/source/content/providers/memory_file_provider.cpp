#include "content/providers/memory_file_provider.hpp"
#include "content/providers/file_provider.hpp"
#include "content/popups/popup_text_input.hpp"

#include <cstring>

#include <hex/api/imhex_api.hpp>
#include <hex/api/localization.hpp>
#include <hex/api/event.hpp>

#include <wolv/io/file.hpp>

namespace hex::plugin::builtin {

    bool MemoryFileProvider::open() {
        if (this->m_data.empty()) {
            this->m_data.resize(1);
            this->markDirty();
        }

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
        if (!this->m_name.empty())
            return;

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

    [[nodiscard]] std::string MemoryFileProvider::getName() const {
        if (this->m_name.empty())
            return LangEntry("hex.builtin.provider.mem_file.unsaved");
        else
            return this->m_name;
    }

    std::vector<MemoryFileProvider::MenuEntry> MemoryFileProvider::getMenuEntries() {
        return {
            MenuEntry { LangEntry("hex.builtin.provider.mem_file.rename"), [this]() { this->renameFile(); } }
        };
    }

    std::pair<Region, bool> MemoryFileProvider::getRegionValidity(u64 address) const {
        address -= this->getBaseAddress();

        if (address < this->getActualSize())
            return { Region { this->getBaseAddress() + address, this->getActualSize() - address }, true };
        else
            return { Region::Invalid(), false };
    }

    void MemoryFileProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        this->m_data = settings["data"].get<std::vector<u8>>();
        this->m_name = settings["name"].get<std::string>();
    }

    [[nodiscard]] nlohmann::json MemoryFileProvider::storeSettings(nlohmann::json settings) const {
        settings["data"] = this->m_data;
        settings["name"] = this->m_name;

        return Provider::storeSettings(settings);
    }

    void MemoryFileProvider::renameFile() {
        PopupTextInput::open("hex.builtin.provider.mem_file.rename"_lang, "hex.builtin.provider.mem_file.rename.desc"_lang, [this](const std::string &name) {
            this->m_name = name;
        });
    }

}
