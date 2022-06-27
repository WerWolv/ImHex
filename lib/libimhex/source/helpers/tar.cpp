#include <hex/helpers/tar.hpp>
#include <hex/helpers/literals.hpp>
#include <hex/helpers/file.hpp>

namespace hex {

    using namespace hex::literals;

    Tar::Tar(const std::fs::path &path, Mode mode) {
        if (mode == Tar::Mode::Read)
            mtar_open(&this->m_ctx, path.string().c_str(), "r");
        else if (mode == Tar::Mode::Write)
            mtar_open(&this->m_ctx, path.string().c_str(), "a");
        else if (mode == Tar::Mode::Create)
            mtar_open(&this->m_ctx, path.string().c_str(), "w");
    }

    Tar::~Tar() {
        mtar_finalize(&this->m_ctx);
        mtar_close(&this->m_ctx);
    }

    std::vector<std::fs::path> Tar::listEntries() {
        std::vector<std::fs::path> result;

        const std::string PaxHeaderName = "@PaxHeader";
        mtar_header_t header;
        while (mtar_read_header(&this->m_ctx, &header) != MTAR_ENULLRECORD) {
            if (header.name != PaxHeaderName) {
                result.emplace_back(header.name);
            }

            mtar_next(&this->m_ctx);
        }

        return result;
    }

    std::vector<u8> Tar::read(const std::fs::path &path) {
        mtar_header_t header;
        mtar_find(&this->m_ctx, path.string().c_str(), &header);

        std::vector<u8> result(header.size, 0x00);
        mtar_read_data(&this->m_ctx, result.data(), result.size());

        return result;
    }

    void Tar::write(const std::fs::path &path, const std::vector<u8> &data) {
        if (path.has_parent_path()) {
            std::fs::path pathPart;
            for (const auto &part : path.parent_path()) {
                pathPart /= part;
                mtar_write_dir_header(&this->m_ctx, pathPart.string().c_str());
            }
        }

        mtar_write_file_header(&this->m_ctx, path.string().c_str(), data.size());
        mtar_write_data(&this->m_ctx, data.data(), data.size());
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