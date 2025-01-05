#include <hex.hpp>
#include <hex/plugin.hpp>

#include <hex/api/content_registry.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <wolv/utils/guards.hpp>

#include <vector>
#include <optional>

#if IMHEX_FEATURE_ENABLED(ZLIB)
    #include <zlib.h>
#endif
#if IMHEX_FEATURE_ENABLED(BZIP2)
    #include <bzlib.h>
#endif
#if IMHEX_FEATURE_ENABLED(LIBLZMA)
    #include <lzma.h>
#endif
#if IMHEX_FEATURE_ENABLED(ZSTD)
    #include <zstd.h>
#endif
#if IMHEX_FEATURE_ENABLED(LZ4)
    #include <lz4.h>
    #include <lz4frame.h>
#endif


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

        /* zlib_decompress(compressed_pattern, section_id) */
        ContentRegistry::PatternLanguage::addFunction(nsHexDec, "zlib_decompress", FunctionParameterCount::exactly(3), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
            #if IMHEX_FEATURE_ENABLED(ZLIB)
                auto compressedData = getCompressedData(evaluator, params[0]);
                auto &section = evaluator->getSection(params[1].toUnsigned());
                auto windowSize = params[2].toUnsigned();

                z_stream stream = { };
                if (inflateInit2(&stream, windowSize) != Z_OK) {
                    return false;
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
                        return false;

                    if (stream.avail_out != 0)
                        break;

                    const auto prevSectionSize = section.size();
                    section.resize(prevSectionSize * 2);
                    stream.next_out  = section.data() + prevSectionSize;
                    stream.avail_out = prevSectionSize;
                }

                return true;
            #else
                std::ignore = evaluator;
                std::ignore = params;
                err::E0012.throwError("hex::dec::zlib_decompress is not available. Please recompile ImHex with zlib support.");
            #endif
        });

        /* bzip_decompress(compressed_pattern, section_id) */
        ContentRegistry::PatternLanguage::addFunction(nsHexDec, "bzip_decompress", FunctionParameterCount::exactly(2), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
            #if IMHEX_FEATURE_ENABLED(BZIP2)
                auto compressedData = getCompressedData(evaluator, params[0]);
                auto &section = evaluator->getSection(params[1].toUnsigned());

                bz_stream stream = { };
                if (BZ2_bzDecompressInit(&stream, 0, 1) != Z_OK) {
                    return false;
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
                        return false;

                    if (stream.avail_out != 0)
                        break;

                    const auto prevSectionSize = section.size();
                    section.resize(prevSectionSize * 2);
                    stream.next_out  = reinterpret_cast<char*>(section.data()) + prevSectionSize;
                    stream.avail_out = prevSectionSize;
                }

                return true;
            #else
                std::ignore = evaluator;
                std::ignore = params;
                err::E0012.throwError("hex::dec::bzlib_decompress is not available. Please recompile ImHex with bzip2 support.");
            #endif

        });

        /* lzma_decompress(compressed_pattern, section_id) */
        ContentRegistry::PatternLanguage::addFunction(nsHexDec, "lzma_decompress", FunctionParameterCount::exactly(2), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
            #if IMHEX_FEATURE_ENABLED(LIBLZMA)
                auto compressedData = getCompressedData(evaluator, params[0]);
                auto &section = evaluator->getSection(params[1].toUnsigned());

                lzma_stream stream = LZMA_STREAM_INIT;
                constexpr int64_t memlimit = 0x40000000;  // 1GiB
                if (lzma_auto_decoder(&stream, memlimit, LZMA_IGNORE_CHECK) != LZMA_OK) {
                    return false;
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
                    if (res == LZMA_STREAM_END) {
                        section.resize(section.size() - stream.avail_out);
                        break;
                    }

                    if (res == LZMA_MEMLIMIT_ERROR) {
                        auto usage = lzma_memusage(&stream);
                        evaluator->getConsole().log(pl::core::LogConsole::Level::Warning, fmt::format("lzma_decompress memory usage {} bytes would exceed the limit ({} bytes), aborting", usage, memlimit));
                        return false;
                    }

                    if (res != LZMA_OK)
                        return false;

                    if (stream.avail_out != 0)
                        break;

                    const auto prevSectionSize = section.size();
                    section.resize(prevSectionSize * 2);
                    stream.next_out  = section.data() + prevSectionSize;
                    stream.avail_out = prevSectionSize;
                }

                return true;
            #else
                std::ignore = evaluator;
                std::ignore = params;
                err::E0012.throwError("hex::dec::lzma_decompress is not available. Please recompile ImHex with liblzma support.");
            #endif
        });

        /* zstd_decompress(compressed_pattern, section_id) */
        ContentRegistry::PatternLanguage::addFunction(nsHexDec, "zstd_decompress", FunctionParameterCount::exactly(2), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
            #if IMHEX_FEATURE_ENABLED(ZSTD)
                auto compressedData = getCompressedData(evaluator, params[0]);
                auto &section = evaluator->getSection(params[1].toUnsigned());

                ZSTD_DCtx* dctx = ZSTD_createDCtx();
                if (dctx == nullptr) {
                    return false;
                }

                ON_SCOPE_EXIT {
                    ZSTD_freeDCtx(dctx);
                };

                const u8* source = compressedData.data();
                size_t sourceSize = compressedData.size();

                size_t blockSize = ZSTD_getFrameContentSize(source, sourceSize);

                if (blockSize == ZSTD_CONTENTSIZE_ERROR) {
                    return false;
                }

                if (blockSize == ZSTD_CONTENTSIZE_UNKNOWN) {
                    // Data uses stream compression
                    ZSTD_inBuffer dataIn = { (void*)source, sourceSize, 0 };

                    size_t outSize = ZSTD_DStreamOutSize();
                    std::vector<u8> outVec(outSize);
                    const u8* out = outVec.data();

                    size_t lastRet = 0;
                    while (dataIn.pos < dataIn.size) {
                        ZSTD_outBuffer dataOut = { (void*)out, outSize, 0 };

                        size_t ret = ZSTD_decompressStream(dctx, &dataOut, &dataIn);
                        if (ZSTD_isError(ret)) {
                            return false;
                        }
                        lastRet = ret;

                        size_t sectionSize = section.size();
                        section.resize(sectionSize + dataOut.pos);
                        std::memcpy(section.data() + sectionSize, out, dataOut.pos);
                    }

                    // Incomplete frame
                    if (lastRet != 0) {
                        return false;
                    }
                } else {
                    section.resize(section.size() + blockSize);

                    size_t ret = ZSTD_decompressDCtx(dctx, section.data() + section.size() - blockSize, blockSize, source, sourceSize);

                    if (ZSTD_isError(ret)) {
                        return false;
                    }
                }

                return true;
            #else
                std::ignore = evaluator;
                std::ignore = params;
                err::E0012.throwError("hex::dec::zstd_decompress is not available. Please recompile ImHex with zstd support.");
            #endif
        });

        /* lz4_decompress(compressed_pattern, section_id) */
        ContentRegistry::PatternLanguage::addFunction(nsHexDec, "lz4_decompress", FunctionParameterCount::exactly(3), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
            #if IMHEX_FEATURE_ENABLED(LZ4)
                auto compressedData = getCompressedData(evaluator, params[0]);
                auto &section = evaluator->getSection(params[1].toUnsigned());
                bool frame = params[2].toBoolean();

                if (frame) {
                    LZ4F_decompressionContext_t dctx;
                    LZ4F_errorCode_t err = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
                    if (LZ4F_isError(err)) {
                       return false;
                    }

                    std::vector<u8> outBuffer(1024 * 1024);

                    const u8* sourcePointer = compressedData.data();
                    size_t srcSize = compressedData.size();

                    while (srcSize > 0) {
                        u8* dstPtr = outBuffer.data();
                        size_t dstCapacity = outBuffer.size();

                        size_t ret = LZ4F_decompress(dctx, dstPtr, &dstCapacity, sourcePointer, &srcSize, nullptr);
                        if (LZ4F_isError(ret)) {
                            LZ4F_freeDecompressionContext(dctx);
                            return false;
                        }

                        section.insert(section.end(), outBuffer.begin(), outBuffer.begin() + dstCapacity);
                        sourcePointer += (compressedData.size() - srcSize);
                    }

                    LZ4F_freeDecompressionContext(dctx);
                } else {
                    section.resize(1024 * 1024);

                    while (true) {
                        auto decompressedSize = LZ4_decompress_safe(reinterpret_cast<const char*>(compressedData.data()), reinterpret_cast<char *>(section.data()), compressedData.size(), static_cast<int>(section.size()));

                        if (decompressedSize < 0) {
                            return false;
                        } else if (decompressedSize > 0) {
                            // Successful decompression
                            section.resize(decompressedSize);
                            break;
                        } else {
                            // Buffer too small, resize and try again
                            section.resize(section.size() * 2);
                        }
                    }
                }


                return true;
            #else
                std::ignore = evaluator;
                std::ignore = params;
                err::E0012.throwError("hex::dec::lz4_decompress is not available. Please recompile ImHex with liblz4 support.");
            #endif
        });
    }

}
