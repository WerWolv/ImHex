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
        explicit FileProvider(std::string_view path);
        ~FileProvider() override;

        bool isAvailable() override;
        bool isReadable() override;
        bool isWritable() override;

        void read(u64 offset, void *buffer, size_t size, bool overlays) override;
        void write(u64 offset, const void *buffer, size_t size) override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        size_t getActualSize() override;

        std::vector<std::pair<std::string, std::string>> getDataInformation() override;

    private:
        #if defined(OS_WINDOWS)
        HANDLE m_file;
        HANDLE m_mapping;
        #else
        int m_file;
        #endif
        std::string m_path;
        void *m_mappedFile;
        size_t m_fileSize;

        bool m_fileStatsValid = false;
        struct stat m_fileStats = { 0 };

        bool m_readable, m_writable;
    };

}