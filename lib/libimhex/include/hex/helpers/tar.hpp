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
        Tar(const Tar&) = delete;
        Tar(Tar&&) noexcept;

        Tar &operator=(Tar &&other) noexcept;

        void close();

        [[nodiscard]] std::vector<u8> read(const std::fs::path &path);
        [[nodiscard]] std::string readString(const std::fs::path &path);

        void write(const std::fs::path &path, const std::vector<u8> &data);
        void write(const std::fs::path &path, const std::string &data);

        [[nodiscard]] std::vector<std::fs::path> listEntries(const std::fs::path &basePath = "/");
        [[nodiscard]] bool contains(const std::fs::path &path);

        void extract(const std::fs::path &path, const std::fs::path &outputPath);
        void extractAll(const std::fs::path &outputPath);

        [[nodiscard]] bool isValid() const { return this->m_valid; }

    private:
        mtar_t m_ctx = { };

        bool m_valid = false;
    };

}