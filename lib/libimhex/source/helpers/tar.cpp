#include <hex/helpers/tar.hpp>
#include <hex/helpers/literals.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/fmt.hpp>

#include <wolv/io/file.hpp>

#include <microtar.h>

namespace hex {

    using namespace hex::literals;

    Tar::Tar(const std::fs::path &path, Mode mode) {
        int tarError = MTAR_ESUCCESS;

        // Explicitly create file so a short path gets generated
        if (mode == Mode::Create) {
            wolv::io::File file(path, wolv::io::File::Mode::Create);
            file.flush();
        }

        m_ctx = std::make_unique<mtar_t>();

        auto shortPath = wolv::io::fs::toShortPath(path);
        if (mode == Tar::Mode::Read)
            tarError = mtar_open(m_ctx.get(), shortPath.string().c_str(), "r");
        else if (mode == Tar::Mode::Write)
            tarError = mtar_open(m_ctx.get(), shortPath.string().c_str(), "a");
        else if (mode == Tar::Mode::Create)
            tarError = mtar_open(m_ctx.get(), shortPath.string().c_str(), "w");
        else
            tarError = MTAR_EFAILURE;

        m_path = path;
        m_valid = (tarError == MTAR_ESUCCESS);

        if (!m_valid) {
            m_tarOpenErrno = tarError;
            
            // Hopefully this errno corresponds to the file open call in mtar_open
            m_fileOpenErrno = errno;
        }
    }

    Tar::~Tar() {
        this->close();
    }

    Tar::Tar(hex::Tar &&other) noexcept {
        m_ctx = std::move(other.m_ctx);
        m_path = other.m_path;
        m_valid = other.m_valid;
        m_tarOpenErrno = other.m_tarOpenErrno;
        m_fileOpenErrno = other.m_fileOpenErrno;

        other.m_ctx = { };
        other.m_valid = false;
    }

    Tar &Tar::operator=(Tar &&other) noexcept {
        m_ctx  = std::move(other.m_ctx);
        m_path = std::move(other.m_path);

        m_valid = other.m_valid;
        other.m_valid = false;

        m_tarOpenErrno = other.m_tarOpenErrno;
        m_fileOpenErrno = other.m_fileOpenErrno;
        return *this;
    }

    std::vector<std::fs::path> Tar::listEntries(const std::fs::path &basePath) const {
        std::vector<std::fs::path> result;

        const std::string PaxHeaderName = "@PaxHeader";
        mtar_header_t header;
        while (mtar_read_header(m_ctx.get(), &header) != MTAR_ENULLRECORD) {
            std::fs::path path = header.name;
            if (header.name != PaxHeaderName && wolv::io::fs::isSubPath(basePath, path)) {
                result.emplace_back(header.name);
            }

            mtar_next(m_ctx.get());
        }

        return result;
    }

    bool Tar::contains(const std::fs::path &path) const {
        mtar_header_t header;

        const auto fixedPath = wolv::io::fs::toNormalizedPathString(path);
        return mtar_find(m_ctx.get(), fixedPath.c_str(), &header) == MTAR_ESUCCESS;
    }

    std::string Tar::getOpenErrorString() const {
        return hex::format("{}: {}", mtar_strerror(m_tarOpenErrno), std::strerror(m_fileOpenErrno));
    }

    void Tar::close() {
        if (m_valid) {
            mtar_finalize(m_ctx.get());
            mtar_close(m_ctx.get());
        }

        m_ctx.reset();
        m_valid = false;
    }

    std::vector<u8> Tar::readVector(const std::fs::path &path) const {
        mtar_header_t header;

        const auto fixedPath = wolv::io::fs::toNormalizedPathString(path);
        int ret = mtar_find(m_ctx.get(), fixedPath.c_str(), &header);
        if (ret != MTAR_ESUCCESS){
            log::debug("Failed to read vector from path {} in tarred file {}: {}",
                path.string(), m_path.string(), mtar_strerror(ret));
            return {};
        }
        
        std::vector<u8> result(header.size, 0x00);
        mtar_read_data(m_ctx.get(), result.data(), result.size());

        return result;
    }

    std::string Tar::readString(const std::fs::path &path) const {
        auto result = this->readVector(path);
        return { result.begin(), result.end() };
    }

    void Tar::writeVector(const std::fs::path &path, const std::vector<u8> &data) const {
        if (path.has_parent_path()) {
            std::fs::path pathPart;
            for (const auto &part : path.parent_path()) {
                pathPart /= part;

                auto fixedPath = wolv::io::fs::toNormalizedPathString(pathPart);
                mtar_write_dir_header(m_ctx.get(), fixedPath.c_str());
            }
        }

        const auto fixedPath = wolv::io::fs::toNormalizedPathString(path);
        mtar_write_file_header(m_ctx.get(), fixedPath.c_str(), data.size());
        mtar_write_data(m_ctx.get(), data.data(), data.size());
    }

    void Tar::writeString(const std::fs::path &path, const std::string &data) const {
        this->writeVector(path, { data.begin(), data.end() });
    }

    static void writeFile(mtar_t *ctx, const mtar_header_t *header, const std::fs::path &path) {
        constexpr static u64 BufferSize = 1_MiB;

        wolv::io::File outputFile(path, wolv::io::File::Mode::Create);

        std::vector<u8> buffer;
        for (u64 offset = 0; offset < header->size; offset += BufferSize) {
            buffer.resize(std::min<u64>(BufferSize, header->size - offset));

            mtar_read_data(ctx, buffer.data(), buffer.size());
            outputFile.writeVector(buffer);
        }
    }

    void Tar::extract(const std::fs::path &path, const std::fs::path &outputPath) const {
        mtar_header_t header;
        mtar_find(m_ctx.get(), path.string().c_str(), &header);

        writeFile(m_ctx.get(), &header, outputPath);
    }

    void Tar::extractAll(const std::fs::path &outputPath) const {
        mtar_header_t header;
        while (mtar_read_header(m_ctx.get(), &header) != MTAR_ENULLRECORD) {
            const auto filePath = std::fs::absolute(outputPath / std::fs::path(header.name));

            if (filePath.filename() != "@PaxHeader") {

                std::fs::create_directories(filePath.parent_path());

                writeFile(m_ctx.get(), &header, filePath);
            }

            mtar_next(m_ctx.get());
        }
    }

}