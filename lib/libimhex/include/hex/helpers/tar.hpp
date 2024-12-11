#pragma once

#include <hex.hpp>

#include <hex/helpers/fs.hpp>

#include <memory>

struct mtar_t;

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

        /**
         * @brief get the error string explaining the error that occurred when opening the file.
         * This error is a combination of the tar error and the native file open error
         */
        std::string getOpenErrorString() const;

        [[nodiscard]] std::vector<u8> readVector(const std::fs::path &path) const;
        [[nodiscard]] std::string readString(const std::fs::path &path) const;

        void writeVector(const std::fs::path &path, const std::vector<u8> &data) const;
        void writeString(const std::fs::path &path, const std::string &data) const;

        [[nodiscard]] std::vector<std::fs::path> listEntries(const std::fs::path &basePath = "/") const;
        [[nodiscard]] bool contains(const std::fs::path &path) const;

        void extract(const std::fs::path &path, const std::fs::path &outputPath) const;
        void extractAll(const std::fs::path &outputPath) const;

        [[nodiscard]] bool isValid() const { return m_valid; }

    private:
        std::unique_ptr<mtar_t> m_ctx;
        std::fs::path m_path;

        bool m_valid = false;
        
        // These will be updated when the constructor is called
        int m_tarOpenErrno  = 0;
        int m_fileOpenErrno = 0;
    };

}
