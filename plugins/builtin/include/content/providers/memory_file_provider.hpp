#pragma once

#include <hex/providers/provider.hpp>

namespace hex::plugin::builtin {

    class MemoryFileProvider : public hex::prv::Provider {
    public:
        explicit MemoryFileProvider() = default;
        ~MemoryFileProvider() override = default;

        [[nodiscard]] bool isAvailable() const override { return true; }
        [[nodiscard]] bool isReadable()  const override { return true; }
        [[nodiscard]] bool isWritable()  const override { return !m_readOnly; }
        [[nodiscard]] bool isResizable() const override { return !m_readOnly; }
        [[nodiscard]] bool isSavable()   const override { return m_name.empty(); }
        [[nodiscard]] bool isSavableAsRecent() const override { return false; }

        [[nodiscard]] bool open() override;
        void close() override { }

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] u64 getActualSize() const override { return m_data.size(); }

        void resizeRaw(u64 newSize) override;

        void save() override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<Description> getDataDescription() const override { return { }; }

        std::vector<MenuEntry> getMenuEntries() override;

        [[nodiscard]] UnlocalizedString getTypeName() const override {
            return "hex.builtin.provider.mem_file";
        }

        [[nodiscard]] std::pair<Region, bool> getRegionValidity(u64 address) const override;

        void loadSettings(const nlohmann::json &settings) override;
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override;

        void setReadOnly(bool readOnly) { m_readOnly = readOnly; }

    private:
        void renameFile();

    private:
        std::vector<u8> m_data;
        std::string m_name;
        bool m_readOnly = false;
    };

}