#pragma once

#include "providers/provider.hpp"

#include <string_view>

#include <sys/stat.h>

namespace hex::prv {

    class FileProvider : public Provider {
    public:
        explicit FileProvider(std::string_view path);
        ~FileProvider() override;

        bool isAvailable() override;
        bool isReadable() override;
        bool isWritable() override;

        void read(u64 offset, void *buffer, size_t size) override;
        void write(u64 offset, void *buffer, size_t size) override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, void *buffer, size_t size) override;
        size_t getActualSize() override;

        std::vector<std::pair<std::string, std::string>> getDataInformation() override;

    private:
        FILE *m_file;
        std::string m_path;

        bool m_fileStatsValid = false;
        struct stat m_fileStats = { 0 };

        bool m_readable, m_writable;
    };

}