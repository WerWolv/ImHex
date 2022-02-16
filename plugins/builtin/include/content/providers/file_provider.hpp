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
        explicit FileProvider();
        ~FileProvider() override;

        [[nodiscard]] bool isAvailable() const override;
        [[nodiscard]] bool isReadable() const override;
        [[nodiscard]] bool isWritable() const override;
        [[nodiscard]] bool isResizable() const override;
        [[nodiscard]] bool isSavable() const override;

        void read(u64 offset, void *buffer, size_t size, bool overlays) override;
        void write(u64 offset, const void *buffer, size_t size) override;

        void resize(size_t newSize) override;
        void insert(u64 offset, size_t size) override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] size_t getActualSize() const override;

        void save() override;
        void saveAs(const fs::path &path) override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<std::pair<std::string, std::string>> getDataInformation() const override;

        void setPath(const fs::path &path);

        [[nodiscard]] bool open() override;
        void close() override;

    protected:
#if defined(OS_WINDOWS)
        HANDLE m_file    = INVALID_HANDLE_VALUE;
        HANDLE m_mapping = INVALID_HANDLE_VALUE;
#else
        int m_file = -1;
#endif

        fs::path m_path;
        void *m_mappedFile = nullptr;
        size_t m_fileSize  = 0;

        struct stat m_fileStats = { 0 };
        bool m_fileStatsValid   = false;
        bool m_emptyFile        = false;

        bool m_readable = false, m_writable = false;
    };

}