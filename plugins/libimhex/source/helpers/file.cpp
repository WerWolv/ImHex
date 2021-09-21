#include <hex/helpers/file.hpp>
#include <unistd.h>

namespace hex {

    File::File(const std::string &path, Mode mode) {
        if (mode == File::Mode::Read)
            this->m_file = fopen64(path.c_str(), "rb");
        else if (mode == File::Mode::Write)
            this->m_file = fopen64(path.c_str(), "r+b");

        if (mode == File::Mode::Create || this->m_file == nullptr)
            this->m_file = fopen64(path.c_str(), "w+b");
    }

    File::File() {
        this->m_file = nullptr;
    }

    File::File(File &&other) noexcept {
        this->m_file = other.m_file;
        other.m_file = nullptr;
    }

    File::~File() {
        if (isValid())
            fclose(this->m_file);
    }

    void File::seek(u64 offset) {
        fseeko64(this->m_file, offset, SEEK_SET);
    }

    size_t File::readBuffer(u8 *buffer, size_t size) {
        if (!isValid()) return 0;

        return fread(buffer, size, 1, this->m_file);
    }

    std::vector<u8> File::readBytes(size_t numBytes) {
        if (!isValid()) return { };

        std::vector<u8> bytes(numBytes ?: getSize());
        auto bytesRead = fread(bytes.data(), bytes.size(), 1, this->m_file);

        bytes.resize(bytesRead);

        return bytes;
    }

    std::string File::readString(size_t numBytes) {
        if (!isValid()) return { };

        return reinterpret_cast<char*>(readBytes(numBytes).data());
    }

    void File::write(const u8 *buffer, size_t size) {
        if (!isValid()) return;

        fwrite(buffer, size, 1, this->m_file);
    }

    void File::write(const std::vector<u8> &bytes) {
        if (!isValid()) return;

        fwrite(bytes.data(), bytes.size(), 1, this->m_file);
    }

    void File::write(const std::string &string) {
        if (!isValid()) return;

        fwrite(string.data(), string.size(), 1, this->m_file);
    }

    size_t File::getSize() const {
        if (!isValid()) return 0;

        auto startPos = ftello64(this->m_file);
        fseeko64(this->m_file, 0, SEEK_END);
        size_t size = ftello64(this->m_file);
        fseeko64(this->m_file, startPos, SEEK_SET);

        return size;
    }

    void File::setSize(u64 size) {
        if (!isValid()) return;

        ftruncate64(fileno(this->m_file), size);
    }

}