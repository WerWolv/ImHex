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

namespace hex::prv {

    class FileProvider : public Provider {
    public:
        explicit FileProvider(std::string path);
        ~FileProvider() override;

        bool isAvailable() override;
        bool isReadable() override;
        bool isWritable() override;
        bool isResizable() override;
        bool isSavable() override;

        void read(u64 offset, void *buffer, size_t size, bool overlays) override;
        void write(u64 offset, const void *buffer, size_t size) override;
        void resize(ssize_t newSize) override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        size_t getActualSize() override;

        void save() override;
        void saveAs(const std::string &path) override;

        std::vector<std::pair<std::string, std::string>> getDataInformation() override;

    private:
        #if defined(OS_WINDOWS)
        HANDLE m_file = INVALID_HANDLE_VALUE;
        HANDLE m_mapping = INVALID_HANDLE_VALUE;
        #else
        int m_file = -1;
        #endif

        std::string m_path;
        void *m_mappedFile = nullptr;
        size_t m_fileSize = 0;

        bool m_fileStatsValid = false;
        struct stat m_fileStats = { 0 };

        bool m_readable = false, m_writable = false;

        void open();
        void close();
    };

}