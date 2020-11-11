#pragma once

#include "providers/provider.hpp"

#include <string_view>

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
        size_t getSize() override;

    private:
        FILE *m_file;

        bool m_readable, m_writable;
    };

}