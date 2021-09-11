#pragma once

#include <hex.hpp>

#include <cstdio>
#include <string>
#include <vector>

#if defined(OS_MACOS)
    #define off64_t off_t
    #define fopen64 fopen
    #define fseeko64 fseek
    #define ftello64 ftell
#endif

namespace hex {

    class File {
    public:
        enum class Mode {
            Read,
            Write,
            Create
        };

        explicit File(const std::string &path, Mode mode);
        File();
        ~File();

        bool isValid() { return this->m_file != nullptr; }

        void seek(u64 offset);

        size_t readBuffer(u8 *buffer, size_t size);
        std::vector<u8> readBytes(size_t numBytes = 0);
        std::string readString(size_t numBytes = 0);

        void write(const u8 *buffer, size_t size);
        void write(const std::vector<u8> &bytes);
        void write(const std::string &string);

        size_t getSize();
        void setSize(u64 size);

        auto getHandle() { return this->m_file; }

    private:
        FILE *m_file;
    };

}