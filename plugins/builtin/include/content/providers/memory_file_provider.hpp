#pragma once

#include <hex/providers/provider.hpp>
#include <hex/api/localization.hpp>

namespace hex::plugin::builtin::prv {

    class MemoryFileProvider : public hex::prv::Provider {
    public:
        explicit MemoryFileProvider() = default;
        ~MemoryFileProvider() override = default;

        [[nodiscard]] bool isAvailable() const override { return true; }
        [[nodiscard]] bool isReadable()  const override { return true; }
        [[nodiscard]] bool isWritable()  const override { return true; }
        [[nodiscard]] bool isResizable() const override { return true; }
        [[nodiscard]] bool isSavable()   const override { return true; }

        [[nodiscard]] bool open() override;
        void close() override { }

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] size_t getActualSize() const override { return this->m_data.size(); }

        void resize(size_t newSize) override;
        void insert(u64 offset, size_t size) override;
        void remove(u64 offset, size_t size) override;

        void save() override;
        void saveAs(const std::fs::path &path) override;

        [[nodiscard]] std::string getName() const override { return LangEntry("hex.builtin.provider.mem_file.unsaved"); }
        [[nodiscard]] std::vector<std::pair<std::string, std::string>> getDataInformation() const override { return { }; }

        [[nodiscard]] std::string getTypeName() const override {
            return "hex.builtin.provider.mem_file";
        }

        [[nodiscard]] std::pair<Region, bool> getRegionValidity(u64 address) const override;

        void loadSettings(const nlohmann::json &settings) override { hex::unused(settings); }
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override { return settings; }

    private:
        std::vector<u8> m_data;
    };

}