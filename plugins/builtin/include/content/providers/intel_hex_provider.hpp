#pragma once

#include <hex/providers/provider.hpp>

#include <wolv/container/interval_tree.hpp>

namespace hex::plugin::builtin {

    class IntelHexProvider : public hex::prv::Provider {
    public:
        IntelHexProvider() = default;
        ~IntelHexProvider() override = default;

        [[nodiscard]] bool isAvailable() const override { return m_dataValid; }
        [[nodiscard]] bool isReadable() const override { return true; }
        [[nodiscard]] bool isWritable() const override { return false; }
        [[nodiscard]] bool isResizable() const override { return false; }
        [[nodiscard]] bool isSavable() const override { return false; }

        void setBaseAddress(u64 address) override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] u64 getActualSize() const override;

        bool open() override;
        void close() override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<Description> getDataDescription() const override;

        void loadSettings(const nlohmann::json &settings) override;
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override;

        [[nodiscard]] UnlocalizedString getTypeName() const override {
            return "hex.builtin.provider.intel_hex";
        }

        [[nodiscard]] bool hasFilePicker() const override { return true; }
        [[nodiscard]] bool handleFilePicker() override;

        std::pair<Region, bool> getRegionValidity(u64 address) const override;

    protected:
        bool m_dataValid = false;
        size_t m_dataSize = 0x00;
        wolv::container::IntervalTree<std::vector<u8>> m_data;

        std::fs::path m_sourceFilePath;
    };

}