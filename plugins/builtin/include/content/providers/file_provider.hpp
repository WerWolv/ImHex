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

        bool isAvailable() const override;
        bool isReadable() const override;
        bool isWritable() const override;
        bool isResizable() const override;
        bool isSavable() const override;

        void read(u64 offset, void *buffer, size_t size, bool overlays) override;
        void write(u64 offset, const void *buffer, size_t size) override;
        void resize(ssize_t newSize) override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        size_t getActualSize() const override;

        void save() override;
        void saveAs(const std::string &path) override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<std::pair<std::string, std::string>> getDataInformation() const override;

        void open(const std::string &path);
        void close();

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
    };

}