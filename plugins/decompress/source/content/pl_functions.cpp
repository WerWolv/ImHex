#include <hex.hpp>
#include <hex/plugin.hpp>

#include <hex/api/content_registry/pattern_language.hpp>
#include <hex/helpers/literals.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <wolv/utils/guards.hpp>

#include <array>
#include <span>
#include <vector>
#include <optional>
#include <cstring>
#include <algorithm>

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
                auto &section = evaluator->getSection(u64(params[1].toUnsigned()));
                auto windowSize = u64(params[2].toUnsigned());

                z_stream stream = { };
                if (inflateInit2(&stream, windowSize) != Z_OK) {
                    return u128(0);
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
                    if (res != Z_OK) {
                        section.resize(section.size() - stream.avail_out);
                        return u128(stream.next_in - compressedData.data());
                    }

                    if (stream.avail_out != 0)
                        break;

                    const auto prevSectionSize = section.size();
                    section.resize(prevSectionSize * 2);
                    stream.next_out  = section.data() + prevSectionSize;
                    stream.avail_out = prevSectionSize;
                }

                return u128(stream.next_in - compressedData.data());
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
                auto &section = evaluator->getSection(u64(params[1].toUnsigned()));

                bz_stream stream = { };
                if (BZ2_bzDecompressInit(&stream, 0, 1) != Z_OK) {
                    return u128(0);
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
                    if (res != BZ_OK) {
                        section.resize(section.size() - stream.avail_out);
                        return u128(reinterpret_cast<const u8*>(stream.next_in) - compressedData.data());
                    }

                    if (stream.avail_out != 0)
                        break;

                    const auto prevSectionSize = section.size();
                    section.resize(prevSectionSize * 2);
                    stream.next_out  = reinterpret_cast<char*>(section.data()) + prevSectionSize;
                    stream.avail_out = prevSectionSize;
                }

                return u128(reinterpret_cast<const u8*>(stream.next_in) - compressedData.data());
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
                auto &section = evaluator->getSection(u64(params[1].toUnsigned()));

                lzma_stream stream = LZMA_STREAM_INIT;
                constexpr static i64 MemoryLimit = 0x40000000;  // 1GiB
                if (lzma_auto_decoder(&stream, MemoryLimit, LZMA_IGNORE_CHECK) != LZMA_OK) {
                    return u128(0);
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
                        evaluator->getConsole().log(pl::core::LogConsole::Level::Warning, fmt::format("lzma_decompress memory usage {} bytes would exceed the limit ({} bytes), aborting", usage, MemoryLimit));

                        section.resize(section.size() - stream.avail_out);
                        return u128(stream.next_in - compressedData.data());
                    }

                    if (res != LZMA_OK) {
                        section.resize(section.size() - stream.avail_out);
                        return u128(stream.next_in - compressedData.data());
                    }

                    if (stream.avail_out != 0)
                        break;

                    const auto prevSectionSize = section.size();
                    section.resize(prevSectionSize * 2);
                    stream.next_out  = section.data() + prevSectionSize;
                    stream.avail_out = prevSectionSize;
                }

                return u128(stream.next_in - compressedData.data());
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
                auto &section = evaluator->getSection(i64(params[1].toUnsigned()));

                ZSTD_DCtx* dctx = ZSTD_createDCtx();
                if (dctx == nullptr) {
                    return u128(0);
                }

                ON_SCOPE_EXIT {
                    ZSTD_freeDCtx(dctx);
                };

                const u8* source = compressedData.data();
                size_t sourceSize = compressedData.size();

                size_t blockSize = ZSTD_getFrameContentSize(source, sourceSize);

                if (blockSize == ZSTD_CONTENTSIZE_ERROR) {
                    return u128(0);
                }

                if (blockSize == ZSTD_CONTENTSIZE_UNKNOWN) {
                    // Data uses stream compression
                    ZSTD_inBuffer dataIn = { static_cast<const void*>(source), sourceSize, 0 };

                    size_t outSize = ZSTD_DStreamOutSize();
                    std::vector<u8> outVec(outSize);
                    u8* out = outVec.data();

                    size_t lastRet = 0;
                    while (dataIn.pos < dataIn.size) {
                        ZSTD_outBuffer dataOut = { reinterpret_cast<void*>(out), outSize, 0 };

                        size_t ret = ZSTD_decompressStream(dctx, &dataOut, &dataIn);
                        if (ZSTD_isError(ret)) {
                            section.resize(section.size() - (dataOut.size - dataOut.pos));
                            return i128(dataIn.pos);
                        }
                        lastRet = ret;

                        size_t sectionSize = section.size();
                        section.resize(sectionSize + dataOut.pos);
                        std::memcpy(section.data() + sectionSize, out, dataOut.pos);
                    }

                    // Incomplete frame
                    if (lastRet != 0) {
                        return i128(dataIn.pos);
                    }
                } else {
                    section.resize(section.size() + blockSize);

                    size_t ret = ZSTD_decompressDCtx(dctx, section.data() + section.size() - blockSize, blockSize, source, sourceSize);

                    if (ZSTD_isError(ret)) {
                        return u128(0);
                    }
                }

                return i128(sourceSize);
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
                auto &section = evaluator->getSection(u64(params[1].toUnsigned()));
                bool frame = params[2].toBoolean();

                if (frame) {
                    LZ4F_decompressionContext_t dctx;
                    LZ4F_errorCode_t err = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
                    if (LZ4F_isError(err)) {
                       return u128(0);
                    }

                    std::vector<u8> outBuffer(1024 * 1024);

                    const u8* sourcePointer = compressedData.data();
                    size_t srcRemaining = compressedData.size();
                    size_t ret = -1;

                    while (ret != 0) {
                        u8* dstPtr = outBuffer.data();
                        size_t dstCapacity = outBuffer.size();
                        size_t srcSize = srcRemaining;
                        
                        ret = LZ4F_decompress(dctx, dstPtr, &dstCapacity, sourcePointer, &srcSize, nullptr);
                        if (LZ4F_isError(ret)) {
                            LZ4F_freeDecompressionContext(dctx);
                            return u128(sourcePointer - compressedData.data());
                        }

                        section.insert(section.end(), outBuffer.begin(), outBuffer.begin() + dstCapacity);
                        sourcePointer += srcSize;
                        srcRemaining -= srcSize;
                    }

                    LZ4F_freeDecompressionContext(dctx);

                    return u128(sourcePointer - compressedData.data());
                } else {
                    section.resize(1024 * 1024);

                    while (true) {
                        auto decompressedSize = LZ4_decompress_safe(reinterpret_cast<const char*>(compressedData.data()), reinterpret_cast<char *>(section.data()), compressedData.size(), static_cast<int>(section.size()));

                        if (decompressedSize < 0) {
                            return u128(0);
                        } else if (decompressedSize > 0) {
                            // Successful decompression
                            section.resize(decompressedSize);
                            return i128(compressedData.size());
                        } else {
                            // Buffer too small, resize and try again
                            section.resize(section.size() * 2);
                        }
                    }
                }
            #else
                std::ignore = evaluator;
                std::ignore = params;
                err::E0012.throwError("hex::dec::lz4_decompress is not available. Please recompile ImHex with liblz4 support.");
            #endif
        });

        /* lzf_decompress(compressed_pattern, section_id) */
        ContentRegistry::PatternLanguage::addFunction(nsHexDec, "lzf_decompress", FunctionParameterCount::exactly(2), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
            // original liblzf implementation: http://software.schmorp.de/pkg/liblzf.html (use Wayback Machine to access it)
            using namespace hex::literals;

            const auto input = getCompressedData(evaluator, params[0]);
            auto &output = evaluator->getSection(u64(params[1].toUnsigned()));

            // decompressed size heuristic with a limit
            constexpr size_t InitialSizeLimit = 1_GiB;
            output.resize(std::min(input.size() * 12, InitialSizeLimit));

            size_t inputIdx = 0;
            size_t outputIdx = 0;

            while (inputIdx < input.size()) {
                const u8 ctrl = input[inputIdx++];

                if (ctrl < 32u) { // literal run
                    const size_t len = ctrl + 1;
                    if (inputIdx + len > input.size()) {
                        return u128(0); // error: out of bounds
                    }
                    if (outputIdx + len > output.size()) {
                        output.resize(outputIdx + len);
                    }
                    std::memcpy(output.data() + outputIdx, input.data() + inputIdx, len);
                    outputIdx += len;
                    inputIdx += len;
                } else { // back reference
                    size_t len = ctrl >> 5;
                    size_t offset = ((ctrl & 0x1F) << 8) + 1;

                    if (len == 7) {
                        if (inputIdx >= input.size()) {
                            return u128(0); // error: out of bounds
                        }
                        len += input[inputIdx++];
                    }
                    if (inputIdx >= input.size()) {
                        return u128(0); // error: out of bounds
                    }
                    offset += input[inputIdx++];
                    len += 2;

                    if (offset > outputIdx) {
                        return u128(0); // error: back reference before start of output
                    }
                    if (outputIdx + len > output.size()) {
                        output.resize(outputIdx + len);
                    }
                    if (offset >= len) {
                        // no overlapping
                        std::memcpy(output.data() + outputIdx, output.data() + outputIdx - offset, len);
                    } else {
                        // overlapping, forward copy byte-by-byte to repeat newly copied bytes (cannot use memmove here)
                        for (size_t i = 0; i < len; ++i) {
                            output[outputIdx + i] = output[outputIdx - offset + i];
                        }
                    }
                    outputIdx += len;
                }
            }
            output.resize(outputIdx);
            return u128(input.size()); // return number of bytes read from the input
        });

        /* dcl_decompress(compressed_pattern, section_id) */
        ContentRegistry::PatternLanguage::addFunction(nsHexDec, "dcl_decompress", FunctionParameterCount::exactly(2), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
            // PKWARE Data Compression Library "implode" data, as decoded by blast.c from zlib's contrib folder
            const auto input = getCompressedData(evaluator, params[0]);
            auto &output = evaluator->getSection(u64(params[1].toUnsigned()));
            output.clear();

            size_t inputIdx = 0;
            u32 bitBuffer = 0;
            u8 bitCount = 0;
            bool outOfInput = false;

            // Bits are read starting with the least significant bit of each byte
            auto bits = [&](u8 count) -> u32 {
                while (bitCount < count) {
                    if (inputIdx >= input.size()) {
                        outOfInput = true;
                        return 0;
                    }
                    bitBuffer |= u32(input[inputIdx++]) << bitCount;
                    bitCount += 8;
                }
                const u32 value = bitBuffer & ((1U << count) - 1);
                bitBuffer >>= count;
                bitCount -= count;
                return value;
            };

            // Canonical Huffman codes, given as runs of code lengths: each byte
            // holds a length in its low nibble and a repeat count minus one in its high nibble
            struct Huffman {
                std::array<u16, 14> count = { };
                std::array<u16, 256> symbol = { };
            };
            auto construct = [](std::span<const u8> compact) {
                Huffman huffman;
                std::array<u8, 256> lengths = { };
                size_t symbols = 0;
                for (u8 byte : compact) {
                    for (u8 i = 0; i <= byte >> 4; i += 1)
                        lengths[symbols++] = byte & 0x0F;
                }

                for (size_t i = 0; i < symbols; i += 1)
                    huffman.count[lengths[i]] += 1;

                std::array<u16, 14> offsets = { };
                for (size_t length = 1; length < offsets.size() - 1; length += 1)
                    offsets[length + 1] = offsets[length] + huffman.count[length];
                for (size_t i = 0; i < symbols; i += 1) {
                    if (lengths[i] != 0)
                        huffman.symbol[offsets[lengths[i]]++] = i;
                }

                return huffman;
            };

            // Codes are stored with their bits inverted, most significant bit first
            auto decode = [&](const Huffman &huffman) -> std::optional<u16> {
                i32 code = 0, first = 0, index = 0;
                for (size_t length = 1; length < huffman.count.size(); length += 1) {
                    code |= bits(1) ^ 1;
                    if (outOfInput)
                        return std::nullopt;
                    const i32 count = huffman.count[length];
                    if (code < first + count)
                        return huffman.symbol[index + (code - first)];
                    index += count;
                    first += count;
                    first <<= 1;
                    code <<= 1;
                }
                return std::nullopt;
            };

            constexpr static std::array<u8, 98> LiteralLengths = {
                11, 124, 8, 7, 28, 7, 188, 13, 76, 4, 10, 8, 12, 10, 12, 10, 8, 23, 8,
                9, 7, 6, 7, 8, 7, 6, 55, 8, 23, 24, 12, 11, 7, 9, 11, 12, 6, 7, 22, 5,
                7, 24, 6, 11, 9, 6, 7, 22, 7, 11, 38, 7, 9, 8, 25, 11, 8, 11, 9, 12,
                8, 12, 5, 38, 5, 38, 5, 11, 7, 5, 6, 21, 6, 10, 53, 8, 7, 24, 10, 27,
                44, 253, 253, 253, 252, 252, 252, 13, 12, 45, 12, 45, 12, 61, 12, 45,
                44, 173
            };
            constexpr static std::array<u8, 6> LengthLengths = { 2, 35, 36, 53, 38, 23 };
            constexpr static std::array<u8, 7> DistanceLengths = { 2, 20, 53, 230, 247, 151, 248 };
            constexpr static std::array<u16, 16> LengthBase = { 3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264 };
            constexpr static std::array<u8, 16> LengthExtra = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8 };

            static const Huffman LiteralCodes = construct(LiteralLengths);
            static const Huffman LengthCodes = construct(LengthLengths);
            static const Huffman DistanceCodes = construct(DistanceLengths);

            const u32 codedLiterals = bits(8);
            const u32 dictionaryBits = bits(8);
            if (outOfInput || codedLiterals > 1 || dictionaryBits < 4 || dictionaryBits > 6)
                return u128(0);

            // Stops at the end code, or keeps what was decoded if the data ends early or is invalid
            while (true) {
                if (bits(1) != 0) {
                    const auto lengthSymbol = decode(LengthCodes);
                    if (!lengthSymbol.has_value())
                        break;
                    const u32 length = LengthBase[*lengthSymbol] + bits(LengthExtra[*lengthSymbol]);
                    if (outOfInput || length == 519)
                        break;

                    const u8 shift = length == 2 ? 2 : dictionaryBits;
                    const auto distanceSymbol = decode(DistanceCodes);
                    if (!distanceSymbol.has_value())
                        break;
                    const size_t distance = (size_t(*distanceSymbol) << shift) + bits(shift) + 1;
                    if (outOfInput || distance > output.size())
                        break;

                    // Copies may overlap the bytes they produce
                    for (u32 i = 0; i < length; i += 1)
                        output.push_back(output[output.size() - distance]);
                } else {
                    u32 literal;
                    if (codedLiterals != 0) {
                        const auto symbol = decode(LiteralCodes);
                        if (!symbol.has_value())
                            break;
                        literal = *symbol;
                    } else {
                        literal = bits(8);
                    }
                    if (outOfInput)
                        break;
                    output.push_back(u8(literal));
                }
                if (outOfInput)
                    break;
            }

            return u128(inputIdx);
        });
    }

}
