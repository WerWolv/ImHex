#pragma once

#include <hex/providers/provider.hpp>

#include <IntervalTree.h>

namespace hex::plugin::builtin::prv {

    class IntelHexProvider : public hex::prv::Provider {
    public:
        IntelHexProvider() = default;
        ~IntelHexProvider() override = default;

        [[nodiscard]] bool isAvailable() const override { return this->m_dataValid; }
        [[nodiscard]] bool isReadable() const override { return true; }
        [[nodiscard]] bool isWritable() const override { return false; }
        [[nodiscard]] bool isResizable() const override { return false; }
        [[nodiscard]] bool isSavable() const override { return false; }

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] size_t getActualSize() const override;

        bool open() override;
        void close() override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<std::pair<std::string, std::string>> getDataInformation() const override { return { }; }

        void loadSettings(const nlohmann::json &settings) override { hex::unused(settings); }
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override { return settings; }

        [[nodiscard]] std::string getTypeName() const override {
            return "hex.builtin.provider.intel_hex";
        }

        bool hasFilePicker() const override { return true; }
        bool handleFilePicker() override;

        std::pair<Region, bool> getRegionValidity(u64 address) const override;

    protected:
        bool m_dataValid = false;
        size_t m_dataSize = 0x00;
        interval_tree::IntervalTree<u64, std::vector<u8>> m_data;

        std::fs::path m_sourceFilePath;
    };

}