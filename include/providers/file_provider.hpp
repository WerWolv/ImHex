#pragma once

#include "providers/provider.hpp"

#include <string_view>

namespace hex::prv {

    class FileProvider : public Provider {
    public:
        FileProvider(std::string_view path);
        virtual ~FileProvider();

        virtual bool isAvailable() override;
        virtual bool isReadable() override;
        virtual bool isWritable() override;

        virtual void read(u64 offset, void *buffer, size_t size) override;
        virtual void write(u64 offset, void *buffer, size_t size) override;
        virtual size_t getSize() override;

    private:
        FILE *m_file;

        bool m_readable, m_writable;
    };

}