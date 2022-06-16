#pragma once

#include <hex.hpp>

#include <hex/helpers/fs.hpp>

#include <microtar.h>

namespace hex {

    class Tar {
    public:
        enum class Mode {
            Read,
            Write,
            Create
        };

        Tar() = default;
        Tar(const std::fs::path &path, Mode mode);
        ~Tar();

        std::vector<u8> read(const std::fs::path &path);
        void write(const std::fs::path &path, const std::vector<u8> &data);

        std::vector<std::fs::path> listEntries();

        void extract(const std::fs::path &path, const std::fs::path &outputPath);
        void extractAll(const std::fs::path &outputPath);

    private:
        mtar_t m_ctx = { };
    };

}