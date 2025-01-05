#include <content/providers/view_provider.hpp>

#include <hex/helpers/fmt.hpp>

#include <hex/api/event_manager.hpp>
#include <popups/popup_text_input.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    [[nodiscard]] bool ViewProvider::isAvailable() const {
        if (m_provider == nullptr)
            return false;
        else
            return m_provider->isAvailable();
    }
    [[nodiscard]] bool ViewProvider::isReadable() const {
        if (m_provider == nullptr)
            return false;
        else
            return m_provider->isReadable();
    }
    [[nodiscard]] bool ViewProvider::isWritable() const {
        if (m_provider == nullptr)
            return false;
        else
            return m_provider->isWritable();
    }
    [[nodiscard]] bool ViewProvider::isResizable() const {
        return true;
    }

    [[nodiscard]] bool ViewProvider::isSavable() const {
        if (m_provider == nullptr)
            return false;
        else
            return m_provider->isSavable();
    }

    [[nodiscard]] bool ViewProvider::isSavableAsRecent() const {
        return false;
    }

    void ViewProvider::save() {
        m_provider->save();
    }

    [[nodiscard]] bool ViewProvider::open() {
        if (m_provider == this)
            return false;

        EventProviderClosing::subscribe(this, [this](const prv::Provider *provider, bool*) {
            if (m_provider == provider)
                ImHexApi::Provider::remove(this, false);
        });

        return true;
    }
    void ViewProvider::close() {
        EventProviderClosing::unsubscribe(this);
    }

    void ViewProvider::resizeRaw(u64 newSize) {
        m_size = newSize;
    }
    void ViewProvider::insertRaw(u64 offset, u64 size) {
        if (m_provider == nullptr)
            return;

        m_size += size;
        m_provider->insert(offset + m_startAddress, size);
    }

    void ViewProvider::removeRaw(u64 offset, u64 size) {
        if (m_provider == nullptr)
            return;

        m_size -= size;
        m_provider->remove(offset + m_startAddress, size);
    }

    void ViewProvider::readRaw(u64 offset, void *buffer, size_t size) {
        if (m_provider == nullptr)
            return;

        m_provider->read(offset + m_startAddress, buffer, size);
    }

    void ViewProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        if (m_provider == nullptr)
            return;

        m_provider->write(offset + m_startAddress, buffer, size);
    }

    [[nodiscard]] u64 ViewProvider::getActualSize() const {
        return m_size;
    }

    [[nodiscard]] std::string ViewProvider::getName() const {
        if (!m_name.empty())
            return m_name;

        if (m_provider == nullptr)
            return "View";
        else
            return hex::format("{} View", m_provider->getName());
    }
    [[nodiscard]] std::vector<ViewProvider::Description> ViewProvider::getDataDescription() const {
        if (m_provider == nullptr)
            return { };

        return m_provider->getDataDescription();
    }

    void ViewProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        auto id = settings.at("id").get<u64>();
        m_startAddress = settings.at("start_address").get<u64>();
        m_size = settings.at("size").get<size_t>();

        const auto &providers = ImHexApi::Provider::getProviders();
        auto provider = std::ranges::find_if(providers, [id](const prv::Provider *provider) {
            return provider->getID() == id;
        });

        if (provider == providers.end())
            return;

        m_provider = *provider;
    }

    [[nodiscard]] nlohmann::json ViewProvider::storeSettings(nlohmann::json settings) const {
        settings["id"] = m_provider->getID();
        settings["start_address"] = m_startAddress;
        settings["size"] = m_size;

        return Provider::storeSettings(settings);
    }

    [[nodiscard]] UnlocalizedString ViewProvider::getTypeName() const {
        return "hex.builtin.provider.view";
    }

    void ViewProvider::setProvider(u64 startAddress, size_t size, hex::prv::Provider *provider) {
        m_startAddress = startAddress;
        m_size = size;
        m_provider = provider;
    }

    void ViewProvider::setName(const std::string& name) {
        m_name = name;
    }


    [[nodiscard]] std::pair<Region, bool> ViewProvider::getRegionValidity(u64 address) const {
        address -= this->getBaseAddress();

        if (address < this->getActualSize())
            return { Region { this->getBaseAddress() + address, this->getActualSize() - address }, true };
        else
            return { Region::Invalid(), false };
    }

    std::vector<prv::Provider::MenuEntry> ViewProvider::getMenuEntries() {
        return {
            MenuEntry { Lang("hex.builtin.provider.rename"), ICON_VS_TAG, [this] { this->renameFile(); } }
        };
    }

    void ViewProvider::renameFile() {
        ui::PopupTextInput::open("hex.builtin.provider.rename", "hex.builtin.provider.rename.desc", [this](const std::string &name) {
            m_name = name;
            RequestUpdateWindowTitle::post();
        });
    }

}