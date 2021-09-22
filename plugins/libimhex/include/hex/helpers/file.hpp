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
        File(const File&) = delete;
        File(File &&other) noexcept;

        ~File();

        [[nodiscard]] bool isValid() const { return this->m_file != nullptr; }

        void seek(u64 offset);
        void close();

        size_t readBuffer(u8 *buffer, size_t size);
        std::vector<u8> readBytes(size_t numBytes = 0);
        std::string readString(size_t numBytes = 0);

        void write(const u8 *buffer, size_t size);
        void write(const std::vector<u8> &bytes);
        void write(const std::string &string);

        [[nodiscard]] size_t getSize() const;
        void setSize(u64 size);

        void flush();
        void remove();

        auto getHandle() { return this->m_file; }
        const std::string& getPath() { return this->m_path; }

    private:
        FILE *m_file;
        std::string m_path;
    };

}