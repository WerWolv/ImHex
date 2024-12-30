#include "content/providers/memory_file_provider.hpp"
#include "content/providers/file_provider.hpp"
#include <popups/popup_text_input.hpp>

#include <cstring>

#include <hex/api/imhex_api.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/event_manager.hpp>

#include <wolv/io/file.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    bool MemoryFileProvider::open() {
        if (m_data.empty()) {
            m_data.resize(1);
        }

        return true;
    }

    void MemoryFileProvider::readRaw(u64 offset, void *buffer, size_t size) {
        auto actualSize = this->getActualSize();
        if (actualSize == 0 || (offset + size) > actualSize || buffer == nullptr || size == 0)
            return;

        std::memcpy(buffer, &m_data.front() + offset, size);
    }

    void MemoryFileProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        if ((offset + size) > this->getActualSize() || buffer == nullptr || size == 0)
            return;

        std::memcpy(&m_data.front() + offset, buffer, size);
    }

    void MemoryFileProvider::save() {
        if (!m_name.empty())
            return;

        fs::openFileBrowser(fs::DialogMode::Save, { }, [this](const std::fs::path &path) {
            if (path.empty())
                return;

            this->saveAs(path);

            auto newProvider = hex::ImHexApi::Provider::createProvider("hex.builtin.provider.file", true);

            if (auto fileProvider = dynamic_cast<FileProvider*>(newProvider); fileProvider != nullptr) {
                fileProvider->setPath(path);

                if (!fileProvider->open()) {
                    ImHexApi::Provider::remove(newProvider);
                } else {
                    MovePerProviderData::post(this, fileProvider);

                    fileProvider->markDirty(false);
                    EventProviderOpened::post(newProvider);
                    ImHexApi::Provider::remove(this, true);
                }
            }
        });
    }

    void MemoryFileProvider::resizeRaw(u64 newSize) {
        m_data.resize(newSize);
    }

    [[nodiscard]] std::string MemoryFileProvider::getName() const {
        if (m_name.empty())
            return Lang("hex.builtin.provider.mem_file.unsaved");
        else
            return m_name;
    }

    std::vector<MemoryFileProvider::MenuEntry> MemoryFileProvider::getMenuEntries() {
        return {
            MenuEntry { Lang("hex.builtin.provider.mem_file.rename"), ICON_VS_TAG, [this] { this->renameFile(); } }
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

        m_data = settings["data"].get<std::vector<u8>>();
        m_name = settings["name"].get<std::string>();
        m_readOnly = settings["readOnly"].get<bool>();
    }

    [[nodiscard]] nlohmann::json MemoryFileProvider::storeSettings(nlohmann::json settings) const {
        settings["data"] = m_data;
        settings["name"] = m_name;
        settings["readOnly"] = m_readOnly;

        return Provider::storeSettings(settings);
    }

    void MemoryFileProvider::renameFile() {
        ui::PopupTextInput::open("hex.builtin.provider.rename", "hex.builtin.provider.rename.desc", [this](const std::string &name) {
            m_name = name;
            RequestUpdateWindowTitle::post();
        });
    }

}
