#pragma once

#include <hex/providers/provider.hpp>

#include <string_view>

#include <sys/stat.h>

#if defined(OS_WINDOWS)

    #include <windows.h>

#else

    #include <sys/mman.h>
    #include <unistd.h>
    #include <sys/fcntl.h>

#endif

namespace hex::plugin::builtin::prv {

    class FileProvider : public hex::prv::Provider {
    public:
        FileProvider() = default;;
        ~FileProvider() override = default;;

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
        [[nodiscard]] size_t getRealTimeSize();

        void save() override;
        void saveAs(const std::fs::path &path) override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<std::pair<std::string, std::string>> getDataInformation() const override;

        bool hasFilePicker() const override { return true; }
        bool handleFilePicker() override;

        void setPath(const std::fs::path &path);

        [[nodiscard]] bool open() override;
        void close() override;

        void loadSettings(const nlohmann::json &settings) override;
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override;

        [[nodiscard]] std::string getTypeName() const override {
            return "hex.builtin.provider.file";
        }

        std::pair<Region, bool> getRegionValidity(u64 address) const override;

    protected:
        #if defined(OS_WINDOWS)

            HANDLE m_file    = INVALID_HANDLE_VALUE;

        #else

            int m_file = -1;

        #endif

        std::fs::path m_path;
        void *m_mappedFile = nullptr;
        size_t m_fileSize  = 0;

        struct stat m_fileStats = { };
        bool m_fileStatsValid   = false;
        bool m_emptyFile        = false;

        bool m_readable = false, m_writable = false;
    };

}