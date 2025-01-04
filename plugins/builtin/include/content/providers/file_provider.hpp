#pragma once

#include <hex/providers/provider.hpp>

#include <wolv/io/file.hpp>

#include <set>
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

        void resizeRaw(u64 newSize) override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;

        [[nodiscard]] u64 getActualSize() const override;

        void save() override;
        void saveAs(const std::fs::path &path) override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<Description> getDataDescription() const override;
        std::variant<std::string, i128> queryInformation(const std::string &category, const std::string &argument) override;

        [[nodiscard]] bool hasFilePicker() const override { return true; }
        [[nodiscard]] bool handleFilePicker() override;

        std::vector<MenuEntry> getMenuEntries() override;

        void setPath(const std::fs::path &path);

        [[nodiscard]] bool open() override;
        void close() override;

        void loadSettings(const nlohmann::json &settings) override;
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override;

        [[nodiscard]] UnlocalizedString getTypeName() const override {
            return "hex.builtin.provider.file";
        }

        [[nodiscard]] std::pair<Region, bool> getRegionValidity(u64 address) const override;

    private:
        void convertToMemoryFile();
        void convertToDirectAccess();

        void handleFileChange();

        bool open(bool memoryMapped);

    protected:
        std::fs::path m_path;
        wolv::io::File m_file;
        size_t m_fileSize = 0;

        wolv::io::ChangeTracker m_changeTracker;
        std::vector<u8> m_data;
        bool m_loadedIntoMemory = false;
        bool m_ignoreNextChangeEvent = false;
        bool m_changeEventAcknowledgementPending = false;

        std::optional<struct stat> m_fileStats;

        bool m_readable = false, m_writable = false;

        static std::set<FileProvider*> s_openedFiles;
    };

}