#include "providers/file_provider.hpp"

#include <cstdio>

namespace hex::prv {

    FileProvider::FileProvider(std::string_view path) {
        this->m_file = fopen(path.data(), "r+b");

        this->m_readable = true;
        this->m_writable = true;

        if (this->m_file == nullptr) {
            this->m_file = fopen(path.data(), "rb");
            this->m_writable = false;
        }
    }

    FileProvider::~FileProvider() {
        if (this->m_file != nullptr)
            fclose(this->m_file);
    }


    bool FileProvider::isAvailable() {
        return this->m_file != nullptr;
    }

    bool FileProvider::isReadable() {
        return isAvailable() && this->m_readable;
    }

    bool FileProvider::isWritable() {
        return isAvailable() && this->m_writable;
    }


    void FileProvider::read(u64 offset, void *buffer, size_t size) {
        if ((offset + size) > this->getSize() || buffer == nullptr || size == 0)
            return;

        fseek(this->m_file, offset, SEEK_SET);
        fread(buffer, 1, size, this->m_file);
    }

    void FileProvider::write(u64 offset, void *buffer, size_t size) {
        if (buffer == nullptr || size == 0)
            return;

        fseek(this->m_file, offset, SEEK_SET);
        fwrite(buffer, 1, size, this->m_file);
    }

    size_t FileProvider::getSize() {
        fseek(this->m_file, 0, SEEK_END);
        return ftell(this->m_file);
    }

}