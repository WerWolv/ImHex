#include <hex/helpers/tar.hpp>
#include <hex/helpers/literals.hpp>
#include <hex/helpers/file.hpp>

namespace hex {

    using namespace hex::literals;

    Tar::Tar(const std::fs::path &path, Mode mode) {
        int error = MTAR_ESUCCESS;

        #if defined (OS_WINDOWS)
            if (mode == Tar::Mode::Read)
                error = mtar_wopen(&this->m_ctx, path.c_str(), L"r");
            else if (mode == Tar::Mode::Write)
                error = mtar_wopen(&this->m_ctx, path.c_str(), L"a");
            else if (mode == Tar::Mode::Create)
                error = mtar_wopen(&this->m_ctx, path.c_str(), L"w");
            else
                error = MTAR_EFAILURE;
        #else
            if (mode == Tar::Mode::Read)
                error = mtar_open(&this->m_ctx, path.c_str(), "r");
            else if (mode == Tar::Mode::Write)
                error = mtar_open(&this->m_ctx, path.c_str(), "a");
            else if (mode == Tar::Mode::Create)
                error = mtar_open(&this->m_ctx, path.c_str(), "w");
            else
                error = MTAR_EFAILURE;
        #endif

        this->m_valid = (error == MTAR_ESUCCESS);
    }

    Tar::~Tar() {
        this->close();
    }

    Tar::Tar(hex::Tar &&other) noexcept {
        this->m_ctx = other.m_ctx;
        this->m_valid = other.m_valid;

        other.m_ctx = { };
        other.m_valid = false;
    }

    Tar &Tar::operator=(Tar &&other) noexcept {
        this->m_ctx = other.m_ctx;
        other.m_ctx = { };

        this->m_valid = other.m_valid;
        other.m_valid = false;

        return *this;
    }

    std::vector<std::fs::path> Tar::listEntries(const std::fs::path &basePath) {
        std::vector<std::fs::path> result;

        const std::string PaxHeaderName = "@PaxHeader";
        mtar_header_t header;
        while (mtar_read_header(&this->m_ctx, &header) != MTAR_ENULLRECORD) {
            std::fs::path path = header.name;
            if (header.name != PaxHeaderName && fs::isSubPath(basePath, path)) {
                result.emplace_back(header.name);
            }

            mtar_next(&this->m_ctx);
        }

        return result;
    }

    bool Tar::contains(const std::fs::path &path) {
        mtar_header_t header;
        return mtar_find(&this->m_ctx, path.string().c_str(), &header) == MTAR_ESUCCESS;
    }

    void Tar::close() {
        if (this->m_valid) {
            mtar_finalize(&this->m_ctx);
            mtar_close(&this->m_ctx);
        }

        this->m_ctx = { };
        this->m_valid = false;
    }

    std::vector<u8> Tar::read(const std::fs::path &path) {
        mtar_header_t header;

        auto fixedPath = path.string();
        #if defined(OS_WINDOWS)
            std::replace(fixedPath.begin(), fixedPath.end(), '\\', '/');
        #endif
        mtar_find(&this->m_ctx, fixedPath.c_str(), &header);

        std::vector<u8> result(header.size, 0x00);
        mtar_read_data(&this->m_ctx, result.data(), result.size());

        return result;
    }

    std::string Tar::readString(const std::fs::path &path) {
        auto result = this->read(path);
        return { result.begin(), result.end() };
    }

    void Tar::write(const std::fs::path &path, const std::vector<u8> &data) {
        if (path.has_parent_path()) {
            std::fs::path pathPart;
            for (const auto &part : path.parent_path()) {
                pathPart /= part;

                auto fixedPath = pathPart.string();
                #if defined(OS_WINDOWS)
                    std::replace(fixedPath.begin(), fixedPath.end(), '\\', '/');
                #endif
                mtar_write_dir_header(&this->m_ctx, fixedPath.c_str());
            }
        }

        auto fixedPath = path.string();
        #if defined(OS_WINDOWS)
            std::replace(fixedPath.begin(), fixedPath.end(), '\\', '/');
        #endif
        mtar_write_file_header(&this->m_ctx, fixedPath.c_str(), data.size());
        mtar_write_data(&this->m_ctx, data.data(), data.size());
    }

    void Tar::write(const std::fs::path &path, const std::string &data) {
        this->write(path, std::vector<u8>(data.begin(), data.end()));
    }

    static void writeFile(mtar_t *ctx, mtar_header_t *header, const std::fs::path &path) {
        constexpr static u64 BufferSize = 1_MiB;

        fs::File outputFile(path, fs::File::Mode::Create);

        std::vector<u8> buffer;
        for (u64 offset = 0; offset < header->size; offset += BufferSize) {
            buffer.resize(std::min<u64>(BufferSize, header->size - offset));

            mtar_read_data(ctx, buffer.data(), buffer.size());
            outputFile.write(buffer);
        }
    }

    void Tar::extract(const std::fs::path &path, const std::fs::path &outputPath) {
        mtar_header_t header;
        mtar_find(&this->m_ctx, path.string().c_str(), &header);

        writeFile(&this->m_ctx, &header, outputPath);
    }

    void Tar::extractAll(const std::fs::path &outputPath) {
        mtar_header_t header;
        while (mtar_read_header(&this->m_ctx, &header) != MTAR_ENULLRECORD) {
            const auto filePath = std::fs::absolute(outputPath / std::fs::path(header.name));

            if (filePath.filename() != "@PaxHeader") {

                std::fs::create_directories(filePath.parent_path());

                writeFile(&this->m_ctx, &header, filePath);
            }

            mtar_next(&this->m_ctx);
        }
    }

}