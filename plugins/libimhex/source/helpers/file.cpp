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

    File::~File() {
        if (isValid())
            fclose(this->m_file);
    }

    void File::seek(u64 offset) {
        fseeko64(this->m_file, offset, SEEK_SET);
    }

    std::vector<u8> File::readBytes(size_t numBytes) {
        std::vector<u8> bytes(numBytes ?: getSize());
        auto bytesRead = fread(bytes.data(), bytes.size(), 1, this->m_file);

        bytes.resize(bytesRead);

        return bytes;
    }

    std::string File::readString(size_t numBytes) {
        return reinterpret_cast<char*>(readBytes(numBytes).data());
    }

    void File::write(const std::vector<u8> &bytes) {
        fwrite(bytes.data(), bytes.size(), 1, this->m_file);
    }

    void File::write(const std::string &string) {
        fwrite(string.data(), string.size(), 1, this->m_file);
    }

    size_t File::getSize() {
        auto startPos = ftello64(this->m_file);
        fseeko64(this->m_file, 0, SEEK_END);
        size_t size = ftello64(this->m_file);
        fseeko64(this->m_file, startPos, SEEK_SET);

        return size;
    }

    void File::setSize(u64 size) {
        ftruncate64(fileno(this->m_file), size);
    }

}