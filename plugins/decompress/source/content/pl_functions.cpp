#include <hex.hpp>

#include <hex/api/content_registry.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <archive.h>
#include <archive_entry.h>

#include <wolv/utils/guards.hpp>

#include <vector>
#include <optional>

#include <zlib.h>
#include <bzlib.h>
#include <lzma.h>
#include <zstd.h>

namespace hex::plugin::decompress {

    namespace {

        std::vector<u8> getCompressedData(pl::core::Evaluator *evaluator, const pl::core::Token::Literal &literal) {
            const auto inputPattern = literal.toPattern();

            std::vector<u8> compressedData;
            compressedData.resize(inputPattern->getSize());
            evaluator->readData(inputPattern->getOffset(), compressedData.data(), compressedData.size(), inputPattern->getSection());

            return compressedData;
        }

    }

    void registerPatternLanguageFunctions() {
        using namespace pl::core;
        using FunctionParameterCount = pl::api::FunctionParameterCount;

        const pl::api::Namespace nsHexDec = { "builtin", "hex", "dec" };

        /* decompress() */
        ContentRegistry::PatternLanguage::addFunction(nsHexDec, "decompress", FunctionParameterCount::exactly(2), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
            auto compressedData = getCompressedData(evaluator, params[0]);
            auto &section = evaluator->getSection(params[1].toUnsigned());

            auto inArchive = archive_read_new();
            ON_SCOPE_EXIT {
                archive_read_close(inArchive);
                archive_read_free(inArchive);
            };

            archive_read_support_filter_gzip(inArchive);
            archive_read_support_format_all(inArchive);

            archive_read_open_memory(inArchive, compressedData.data(), compressedData.size());

            archive_entry *entry = nullptr;
            while (archive_read_next_header(inArchive, &entry) == ARCHIVE_OK) {
                const void *block = nullptr;
                size_t size = 0x00;
                i64 offset = 0x00;

                while (archive_read_data_block(inArchive, &block, &size, &offset) == ARCHIVE_OK) {
                    section.resize(section.size() + size);
                    std::memcpy(section.data(), block, size);
                }
            }

            return std::nullopt;
        });

        ContentRegistry::PatternLanguage::addFunction(nsHexDec, "zlib_decompress", FunctionParameterCount::exactly(2), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
            auto compressedData = getCompressedData(evaluator, params[0]);
            auto &section = evaluator->getSection(params[1].toUnsigned());

            z_stream stream = { };
            if (inflateInit(&stream) != Z_OK) {
                return std::nullopt;
            }

            section.resize(100);

            stream.avail_in = compressedData.size();
            stream.avail_out = section.size();
            stream.next_in = compressedData.data();
            stream.next_out = section.data();

            ON_SCOPE_EXIT {
                inflateEnd(&stream);
            };

            while (stream.avail_in != 0) {
                auto res = inflate(&stream, Z_NO_FLUSH);
                if (res == Z_STREAM_END) {
                    section.resize(section.size() - stream.avail_out);
                    break;
                }
                if (res != Z_OK)
                    return std::nullopt;

                if (stream.avail_out != 0)
                    break;

                section.resize(section.size() * 2);
                stream.next_out  = section.data();
                stream.avail_out = section.size();
            }

            return std::nullopt;
        });

        ContentRegistry::PatternLanguage::addFunction(nsHexDec, "bzlib_decompress", FunctionParameterCount::exactly(2), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
            auto compressedData = getCompressedData(evaluator, params[0]);
            auto &section = evaluator->getSection(params[1].toUnsigned());

            bz_stream stream = { };
            if (BZ2_bzDecompressInit(&stream, 0, 1) != Z_OK) {
                return std::nullopt;
            }

            section.resize(100);

            stream.avail_in  = compressedData.size();
            stream.avail_out = section.size();
            stream.next_in   = reinterpret_cast<char*>(compressedData.data());
            stream.next_out  = reinterpret_cast<char*>(section.data());

            ON_SCOPE_EXIT {
                BZ2_bzDecompressEnd(&stream);
            };

            while (stream.avail_in != 0) {
                auto res = BZ2_bzDecompress(&stream);
                if (res == BZ_STREAM_END) {
                    section.resize(section.size() - stream.avail_out);
                    break;
                }
                if (res != BZ_OK)
                    return std::nullopt;

                if (stream.avail_out != 0)
                    break;

                section.resize(section.size() * 2);
                stream.next_out  = reinterpret_cast<char*>(section.data());
                stream.avail_out = section.size();
            }

            return std::nullopt;
        });

        ContentRegistry::PatternLanguage::addFunction(nsHexDec, "lzma_decompress", FunctionParameterCount::exactly(2), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
            auto compressedData = getCompressedData(evaluator, params[0]);
            auto &section = evaluator->getSection(params[1].toUnsigned());

            lzma_stream stream = LZMA_STREAM_INIT;
            if (lzma_auto_decoder(&stream, 0x10000, LZMA_IGNORE_CHECK) != Z_OK) {
                return std::nullopt;
            }

            section.resize(100);

            stream.avail_in = compressedData.size();
            stream.avail_out = section.size();
            stream.next_in = compressedData.data();
            stream.next_out = section.data();

            ON_SCOPE_EXIT {
                lzma_end(&stream);
            };

            while (stream.avail_in != 0) {
                auto res = lzma_code(&stream, LZMA_RUN);
                if (res == BZ_STREAM_END) {
                    section.resize(section.size() - stream.avail_out);
                    break;
                }
                if (res != LZMA_OK && res != LZMA_STREAM_END)
                    return std::nullopt;

                if (stream.avail_out != 0)
                    break;

                section.resize(section.size() * 2);
                stream.next_out = compressedData.data();
                stream.avail_out = compressedData.size();
            }

            return std::nullopt;
        });

        ContentRegistry::PatternLanguage::addFunction(nsHexDec, "zstd_decompress", FunctionParameterCount::exactly(2), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
            auto compressedData = getCompressedData(evaluator, params[0]);
            auto &section = evaluator->getSection(params[1].toUnsigned());

            ZSTD_DCtx* dctx = ZSTD_createDCtx();
            if (dctx == nullptr) {
                return std::nullopt;
            }

            ON_SCOPE_EXIT {
                ZSTD_freeDCtx(dctx);
            };

            const u8* source = compressedData.data();
            size_t sourceSize = compressedData.size();

            do {
                size_t blockSize = ZSTD_getFrameContentSize(source, sourceSize);

                if (blockSize == ZSTD_CONTENTSIZE_ERROR) {
                    ZSTD_freeDCtx(dctx);
                    return std::nullopt;
                }

                section.resize(section.size() + blockSize);

                size_t decodedSize = ZSTD_decompressDCtx(dctx, section.data() + section.size() - blockSize, blockSize, source, sourceSize);

                if (ZSTD_isError(decodedSize)) {
                    return std::nullopt;
                }

                source = source + sourceSize;
                sourceSize = 0;

            } while (sourceSize > 0);

            return std::nullopt;
        });
    }

}