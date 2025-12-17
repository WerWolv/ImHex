#pragma once

#include <fonts/vscode_icons.hpp>
#include <hex/providers/provider.hpp>

namespace hex::plugin::builtin {

    class ViewProvider : public prv::Provider,
                         public prv::IProviderDataDescription,
                         public prv::IProviderMenuItems {
    public:
        ViewProvider() = default;
        ~ViewProvider() override = default;

        [[nodiscard]] bool isAvailable() const override;
        [[nodiscard]] bool isReadable() const override;
        [[nodiscard]] bool isWritable() const override;
        [[nodiscard]] bool isResizable() const override;
        [[nodiscard]] bool isSavable() const override;
        [[nodiscard]] bool isSavableAsRecent() const override;

        void save() override;
        [[nodiscard]] OpenResult open() override;
        void close() override;

        void resizeRaw(u64 newSize) override;
        void insertRaw(u64 offset, u64 size) override;
        void removeRaw(u64 offset, u64 size) override;
        void read(u64 offset, void *buffer, size_t size, bool overlays = true) override;
        void write(u64 offset, const void *buffer, size_t size) override;
        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;

        [[nodiscard]] u64 getActualSize() const override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<Description> getDataDescription() const override;
        [[nodiscard]] UnlocalizedString getTypeName() const override;

        [[nodiscard]] const char* getIcon() const override {
            return ICON_VS_OPEN_PREVIEW;
        }

        void loadSettings(const nlohmann::json &settings) override;
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override;

        u64 getBaseAddress() const override {
            u64 result = m_startAddress;

            if (m_provider != nullptr)
                result += m_provider->getBaseAddress();

            return result;
        }
        void setBaseAddress(u64 address) override { std::ignore = address; }


        void setProvider(u64 startAddress, size_t size, Provider *provider);
        void setName(const std::string &name);

        [[nodiscard]] std::pair<Region, bool> getRegionValidity(u64 address) const override;

        std::vector<MenuEntry> getMenuEntries() override;

        void undo() override { if (m_provider != nullptr) m_provider->undo(); }
        void redo() override { if (m_provider != nullptr) m_provider->redo(); }

        bool canUndo() const override { return m_provider != nullptr && m_provider->canUndo(); }
        bool canRedo() const override { return m_provider != nullptr && m_provider->canRedo(); }

    private:
        void renameFile();

    private:
        std::string m_name;

        u64 m_startAddress = 0x00;
        size_t m_size = 0x00;
        prv::Provider *m_provider = nullptr;
    };

}
