#pragma once

#include <hex/providers/provider.hpp>

namespace hex::plugin::builtin {

    class ViewProvider : public hex::prv::Provider {
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
        [[nodiscard]] bool open() override;
        void close() override;

        void resizeRaw(u64 newSize) override;
        void insertRaw(u64 offset, u64 size) override;
        void removeRaw(u64 offset, u64 size) override;
        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;

        [[nodiscard]] u64 getActualSize() const override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<Description> getDataDescription() const override;
        [[nodiscard]] UnlocalizedString getTypeName() const override;

        void loadSettings(const nlohmann::json &settings) override;
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override;


        void setProvider(u64 startAddress, size_t size, hex::prv::Provider *provider);
        void setName(const std::string &name);

        [[nodiscard]] std::pair<Region, bool> getRegionValidity(u64 address) const override;

        std::vector<MenuEntry> getMenuEntries() override;

    private:
        void renameFile();

    private:
        std::string m_name;

        u64 m_startAddress = 0x00;
        size_t m_size = 0x00;
        prv::Provider *m_provider = nullptr;
    };

}
