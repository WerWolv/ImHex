#pragma once

#include <hex/providers/provider.hpp>
#include <hex/helpers/file.hpp>

#include <string_view>


namespace hex::plugin::builtin {

    class FileProvider : public hex::prv::Provider {
    public:
        FileProvider() = default;
        ~FileProvider() override = default;

        [[nodiscard]] bool isAvailable() const override;
        [[nodiscard]] bool isReadable() const override;
        [[nodiscard]] bool isWritable() const override;
        [[nodiscard]] bool isResizable() const override;
        [[nodiscard]] bool isSavable() const override;

        void read(u64 offset, void *buffer, size_t size, bool overlays) override;
        void write(u64 offset, const void *buffer, size_t size) override;

        void resize(size_t newSize) override;
        void insert(u64 offset, size_t size) override;
        void remove(u64 offset, size_t size) override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] size_t getActualSize() const override;

        void save() override;
        void saveAs(const std::fs::path &path) override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<std::pair<std::string, std::string>> getDataDescription() const override;
        std::variant<std::string, i128> queryInformation(const std::string &category, const std::string &argument) override;

        [[nodiscard]] bool hasFilePicker() const override { return true; }
        [[nodiscard]] bool handleFilePicker() override;

        void setPath(const std::fs::path &path);

        [[nodiscard]] bool open() override;
        void close() override;

        void loadSettings(const nlohmann::json &settings) override;
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override;

        [[nodiscard]] std::string getTypeName() const override {
            return "hex.builtin.provider.file";
        }

        [[nodiscard]] std::pair<Region, bool> getRegionValidity(u64 address) const override;

    protected:
        std::fs::path m_path;
        fs::File m_file;
        size_t m_fileSize = 0;

        std::optional<struct stat> m_fileStats;

        std::mutex m_mutex;
        bool m_readable = false, m_writable = false;
        bool m_openReadOnly = true;

        u64 m_bufferStartAddress;
        std::vector<u8> m_buffer;
    };

}