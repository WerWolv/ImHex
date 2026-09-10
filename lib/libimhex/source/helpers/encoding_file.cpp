#include <hex/helpers/encoding_file.hpp>

#include <hex/helpers/default_paths.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <mutex>
#include <ranges>
#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

namespace hex {

    namespace {

        enum class Utf8CodepointStatus {
            Complete,

            // Too few bytes left for the code point the lead byte announces.
            Incomplete,

            // No number of following bytes can make this a code point.
            Invalid,
        };

        struct Utf8CodepointInfo {
            Utf8CodepointStatus status;
            size_t length;        // both valid only when
            char32_t codepoint;   // status == Complete
        };

        /**
         * @brief Reads the one UTF-8 code point that `text` starts with
         */
        Utf8CodepointInfo readUtf8Codepoint(std::string_view text) {
            if (text.empty())
                return { Utf8CodepointStatus::Incomplete, 0, 0 };

            const auto leadByte = u8(text[0]);
            size_t length;
            char32_t minCodepoint;
            char32_t codepoint;
            if ((leadByte & 0x80) == 0x00)      { length = 1; minCodepoint = 0x0;     codepoint = leadByte & 0x7F; }
            else if ((leadByte & 0xE0) == 0xC0) { length = 2; minCodepoint = 0x80;    codepoint = leadByte & 0x1F; }
            else if ((leadByte & 0xF0) == 0xE0) { length = 3; minCodepoint = 0x800;   codepoint = leadByte & 0x0F; }
            else if ((leadByte & 0xF8) == 0xF0) { length = 4; minCodepoint = 0x10000; codepoint = leadByte & 0x07; }
            else return { Utf8CodepointStatus::Invalid, 0, 0 };

            if (length > text.size())
                return { Utf8CodepointStatus::Incomplete, 0, 0 };

            for (size_t i = 1; i < length; i += 1) {
                if ((u8(text[i]) & 0xC0) != 0x80)
                    return { Utf8CodepointStatus::Invalid, 0, 0 };
                codepoint = (codepoint << 6) | (u8(text[i]) & 0x3F);
            }

            // RFC 3629 bars these three, and only the assembled code point can show them.
            if (codepoint < minCodepoint)
                return { Utf8CodepointStatus::Invalid, 0, 0 };
            if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
                return { Utf8CodepointStatus::Invalid, 0, 0 };
            if (codepoint > 0x10FFFF)
                return { Utf8CodepointStatus::Invalid, 0, 0 };

            return { Utf8CodepointStatus::Complete, length, codepoint };
        }

        // ImHex names its tables after their purpose, not the encoding. Remove an entry once
        // its file is renamed to the standard name in ImHex-Patterns.
        constexpr static auto EncodingNameAliases = std::to_array<std::pair<std::string_view, std::string_view>>({
            { "us-ascii",     "ascii"                  },
            { "utf-8",        "utf8"                   },

            { "cp437",        "ascii_oem"              },
            { "ibm437",       "ascii_oem"              },
            { "cp1252",       "ascii_ansi"             },
            { "windows-1252", "ascii_ansi"             },

            { "iso-8859-2",   "eastern_europe_iso"     },
            { "windows-1250", "eastern_europe_windows" },
            { "iso-8859-5",   "cyrillic_iso"           },
            { "windows-1251", "cyrillic_windows"       },
            { "cp866",        "cyrillic_cp866"         },
            { "ibm866",       "cyrillic_cp866"         },
            { "koi8-r",       "cyrillic_koi8_r"        },
            { "koi8-u",       "cyrillic_koi8_u"        },
            { "iso-8859-6",   "arabic_iso"             },
            { "windows-1256", "arabic_windows"         },
            { "iso-8859-7",   "greek_iso"              },
            { "windows-1253", "greek_windows"          },
            { "iso-8859-8",   "hebrew_iso"             },
            { "windows-1255", "hebrew_windows"         },
            { "iso-8859-9",   "turkish_iso"            },
            { "windows-1254", "turkish_windows"        },
            { "iso-8859-13",  "baltic_iso"             },
            { "windows-1257", "baltic_windows"         },
            { "windows-874",  "thai"                   },
            { "windows-1258", "vietnamese"             },
            { "iso-6937",     "iso_6937"               },

            { "cp037",        "ebcdic"                 },
            { "ibm037",       "ebcdic"                 },

            { "mac",          "macintosh"              },
            { "x-mac-roman",  "macintosh"              },

            { "shift_jis",    "shiftjis"               },
            { "shift-jis",    "shiftjis"               },
            { "sjis",         "shiftjis"               },
            { "cp932",        "ms932"                  },
            { "windows-31j",  "ms932"                  },
            { "euc-jp",       "euc_jp"                 },
            { "euc-kr",       "euc_kr"                 },
            { "jis_x0201",    "jis_x_0201"             },
        });

        /**
         * @brief Reads the right hand side of a table line
         *
         * A `\uXXXX` escape names a code point, so a table can map a byte to a character with no
         * glyph of its own, such as a control code. `\\` is one literal backslash. Any other
         * backslash stands for itself, so a table that already holds one keeps working.
         */
        std::string decodeTableValue(std::string_view value) {
            std::string result;

            for (size_t i = 0; i < value.size(); ) {
                const bool isEscape = value[i] == '\\' && (i + 1) < value.size();

                if (isEscape && value[i + 1] == '\\') {
                    result += '\\';
                    i += 2;
                    continue;
                }

                std::optional<std::string> encoded;
                if (isEscape && value[i + 1] == 'u' && (i + 6) <= value.size()) {
                    u32 codepoint = 0;
                    bool valid = true;
                    for (size_t digit = 0; digit < 4; digit += 1) {
                        const auto hexValue = hexCharToValue(value[i + 2 + digit]);
                        if (!hexValue.has_value()) {
                            valid = false;
                            break;
                        }

                        codepoint = (codepoint << 4) | *hexValue;
                    }

                    // A surrogate half is not a scalar value, so it has no encoding.
                    if (valid && (codepoint < 0xD800 || codepoint > 0xDFFF))
                        encoded = wolv::util::utf32ToUtf8(std::u32string(1, char32_t(codepoint)));
                }

                // Anything unreadable stands for itself, so a stray backslash survives.
                if (!encoded.has_value()) {
                    result += value[i];
                    i += 1;
                    continue;
                }

                result += *encoded;
                i += 6;
            }

            return result;
        }

        /**
         * @brief Finds the encodings/<stem>.tbl file, if there is one
         */
        std::optional<std::fs::path> findEncodingFile(std::string_view stem) {
            // Discards any directory part. A script reaches this with no sandbox prompt.
            const auto fileName = std::fs::path(stem).filename().string() + ".tbl";

            for (const auto &basePath : paths::Encodings.read()) {
                auto path = basePath / fileName;
                if (std::fs::is_regular_file(path))
                    return path;
            }

            return std::nullopt;
        }

        /**
         * @brief Reads one UTF-16 code unit from a byte pair in the given byte order
         */
        u16 readUtf16Unit(std::span<const u8> unitBytes, std::endian endian) {
            u16 unit = u16(unitBytes[0]) | (u16(unitBytes[1]) << 8);
            if (endian == std::endian::big)
                unit = u16((unit << 8) | (unit >> 8));
            return unit;
        }

    }

    bool isSingleCharacter(std::string_view text) {
        const auto info = readUtf8Codepoint(text);
        return info.status == Utf8CodepointStatus::Complete && info.length == text.size();
    }

    bool isValidUtf8(std::string_view text) {
        while (!text.empty()) {
            const auto info = readUtf8Codepoint(text);
            if (info.status != Utf8CodepointStatus::Complete)
                return false;

            text.remove_prefix(info.length);
        }

        return true;
    }

    bool isControlCode(u8 byte) {
        return byte <= 0x1F || byte == 0x7F;
    }

    namespace impl {

        void appendEncodingLineStartAddress(std::vector<u64> &lineStartAddresses, size_t line, u64 nextLineStartAddress) {
            if (line + 1 == lineStartAddresses.size())
                lineStartAddresses.push_back(nextLineStartAddress);
        }

    }

    EncodingFile::EncodingFile() :
        m_mapping(std::make_unique<std::map<size_t, std::map<std::vector<u8>, std::string>>>()),
        m_reverseMapping(std::make_unique<std::map<size_t, std::map<std::string, std::vector<u8>, std::less<>>>>()) {

    }

    EncodingFile::EncodingFile(const hex::EncodingFile &other) {
        m_mapping = std::make_unique<std::map<size_t, std::map<std::vector<u8>, std::string>>>(*other.m_mapping);
        m_reverseMapping = std::make_unique<std::map<size_t, std::map<std::string, std::vector<u8>, std::less<>>>>(*other.m_reverseMapping);
        m_tableContent = other.m_tableContent;
        m_longestSequence = other.m_longestSequence;
        m_shortestSequence = other.m_shortestSequence;
        m_ambiguousEncoding = other.m_ambiguousEncoding;
        m_valid = other.m_valid;
        m_name = other.m_name;
    }

    EncodingFile::EncodingFile(EncodingFile &&other) noexcept {
        m_mapping = std::move(other.m_mapping);
        m_reverseMapping = std::move(other.m_reverseMapping);
        m_tableContent = std::move(other.m_tableContent);
        m_longestSequence = other.m_longestSequence;
        m_shortestSequence = other.m_shortestSequence;
        m_ambiguousEncoding = other.m_ambiguousEncoding;
        m_valid = other.m_valid;
        m_name = std::move(other.m_name);
    }

    EncodingFile::EncodingFile(Type type, const std::fs::path &path) : EncodingFile() {
        auto file = wolv::io::File(path, wolv::io::File::Mode::Read);
        switch (type) {
            case Type::Thingy:
                parse(file.readString());
                break;
            default:
                return;
        }

        {
            m_name = path.stem().string();
            m_name = wolv::util::replaceStrings(m_name, "_", " ");

            if (!m_name.empty())
                m_name[0] = std::toupper(m_name[0]);
        }

        m_valid = true;
    }

    EncodingFile::EncodingFile(Type type, const std::string &content) : EncodingFile() {
        switch (type) {
            case Type::Thingy:
                parse(content);
                break;
            default:
                return;
        }

        m_name = "Unknown";
        m_valid = true;
    }


    EncodingFile &EncodingFile::operator=(const hex::EncodingFile &other) {
        if(this == &other) {
            return *this;
        }
        m_mapping = std::make_unique<std::map<size_t, std::map<std::vector<u8>, std::string>>>(*other.m_mapping);
        m_reverseMapping = std::make_unique<std::map<size_t, std::map<std::string, std::vector<u8>, std::less<>>>>(*other.m_reverseMapping);
        m_tableContent = other.m_tableContent;
        m_longestSequence = other.m_longestSequence;
        m_shortestSequence = other.m_shortestSequence;
        m_ambiguousEncoding = other.m_ambiguousEncoding;
        m_valid = other.m_valid;
        m_name = other.m_name;

        return *this;
    }

    EncodingFile &EncodingFile::operator=(EncodingFile &&other) noexcept {
        m_mapping = std::move(other.m_mapping);
        m_reverseMapping = std::move(other.m_reverseMapping);
        m_tableContent = std::move(other.m_tableContent);
        m_longestSequence = other.m_longestSequence;
        m_shortestSequence = other.m_shortestSequence;
        m_ambiguousEncoding = other.m_ambiguousEncoding;
        m_valid = other.m_valid;
        m_name = std::move(other.m_name);

        return *this;
    }



    std::optional<std::pair<std::string_view, size_t>> EncodingFile::lookup(std::span<const u8> buffer) const {
        for (const auto &[size, mapping] : std::ranges::reverse_view(*m_mapping)) {
            if (size > buffer.size()) continue;

            std::vector key(buffer.begin(), buffer.begin() + size);
            if (const auto entry = mapping.find(key); entry != mapping.end())
                return std::pair<std::string_view, size_t>{ entry->second, size };
        }

        return std::nullopt;
    }

    std::pair<std::string_view, size_t> EncodingFile::getEncodingFor(std::span<const u8> buffer) const {
        return this->lookup(buffer).value_or(std::pair<std::string_view, size_t>{ ".", 1 });
    }

    u64 EncodingFile::getEncodingLengthFor(std::span<u8> buffer) const {
        for (const auto& [size, mapping] : std::ranges::reverse_view(*m_mapping)) {
            if (size > buffer.size()) continue;

            std::vector key(buffer.begin(), buffer.begin() + size);
            if (mapping.contains(key))
                return size;
        }

        return 1;
    }

    std::string EncodingFile::decodeAll(std::span<const u8> buffer) const {
        std::string result;

        while (!buffer.empty()) {
            const auto [character, size] = getEncodingFor(buffer);
            result += character;
            buffer = buffer.subspan(size);
        }

        return result;
    }

    pl::core::DecodeResult EncodingFile::decodeBounded(std::span<const u8> buffer, std::optional<size_t> maxCodepoints) const {
        pl::core::DecodeResult result;

        while (!buffer.empty()) {
            if (maxCodepoints.has_value() && result.codepointCount >= *maxCodepoints) {
                result.stopReason = pl::core::DecodeStop::CodepointLimit;
                return result;
            }

            // Too short for even the shortest mapped sequence.
            if (buffer.size() < m_shortestSequence) {
                result.stopReason = pl::core::DecodeStop::EndOfInput;
                return result;
            }

            const auto entry = lookup(buffer);
            if (!entry.has_value()) {
                result.stopReason = pl::core::DecodeStop::MalformedBytes;
                return result;
            }

            result.text += entry->first;
            result.bytesConsumed += entry->second;
            result.codepointCount += 1;
            buffer = buffer.subspan(entry->second);
        }

        result.stopReason = pl::core::DecodeStop::EndOfInput;
        return result;
    }

    bool EncodingFile::isFullyMapped(std::span<const u8> buffer) const {
        while (!buffer.empty()) {
            const auto entry = lookup(buffer);
            if (!entry.has_value())
                return false;

            buffer = buffer.subspan(entry->second);
        }

        return true;
    }

    std::optional<std::pair<std::vector<u8>, size_t>> EncodingFile::getBytesFor(std::string_view sequence) const {
        for (const auto &[size, mapping] : std::ranges::reverse_view(*m_reverseMapping)) {
            if (size > sequence.size()) continue;

            auto key = sequence.substr(0, size);
            auto iter = mapping.find(key);
            if (iter != mapping.end())
                return std::pair { iter->second, size };
        }

        return std::nullopt;
    }

    std::optional<std::vector<u8>> EncodingFile::encodeAll(std::string_view sequence) const {
        if (!canEncode())
            return std::nullopt;

        std::vector<u8> result;

        while (!sequence.empty()) {
            auto match = getBytesFor(sequence);
            if (!match.has_value())
                return std::nullopt;

            const auto &[bytes, size] = match.value();
            result.insert(result.end(), bytes.begin(), bytes.end());
            sequence = sequence.substr(size);
        }

        return result;
    }


    void EncodingFile::parse(const std::string &content) {
        m_tableContent = content;

        // Every decoded value so far. A repeat makes the encoding ambiguous.
        std::vector<std::string_view> encodedValues;

        for (const auto &line : wolv::util::splitString(m_tableContent, "\n")) {

            std::string from, to;
            {
                auto delimiterPos = line.find('=');

                if (delimiterPos >= line.length())
                    continue;

                from = line.substr(0, delimiterPos);
                to   = line.substr(delimiterPos + 1);

                if (from.empty()) continue;
            }

            auto fromBytes = hex::parseByteString(from);
            if (fromBytes.empty()) continue;

            if (to.length() > 1)
                to = wolv::util::trim(to);
            to = decodeTableValue(to);
            if (to.empty())
                to = " ";

            if (!m_mapping->contains(fromBytes.size()))
                m_mapping->insert({ fromBytes.size(), {} });

            u64 keySize = fromBytes.size();
            u64 valueSize = to.size();

            if (!m_reverseMapping->contains(valueSize))
                m_reverseMapping->insert({ valueSize, {} });

            auto &reverseBucket = (*m_reverseMapping)[valueSize];
            auto existingEntry = reverseBucket.find(to);
            if (existingEntry == reverseBucket.end()) {
                auto iter = reverseBucket.emplace(to, fromBytes).first;
                encodedValues.emplace_back(iter->first);
            } else if (existingEntry->second != fromBytes) {
                // Two byte sequences that give one value conflict. An identical repeated line does not.
                m_ambiguousEncoding = true;
            }

            (*m_mapping)[keySize].insert({ std::move(fromBytes), to });

            m_longestSequence = std::max(m_longestSequence, keySize);
            m_shortestSequence = std::min(m_shortestSequence, keySize);
        }

        // An unmapped byte in 0x00-0x7F is standard ASCII, where it is its own character.
        auto &byteMapping = (*m_mapping)[1];
        for (int byte = 0x00; byte <= 0x7F; byte++) {
            std::vector<u8> key { static_cast<u8>(byte) };
            if (byteMapping.contains(key))
                continue;

            std::string text(1, char(byte));
            byteMapping.emplace(key, text);

            auto &reverseBucket = (*m_reverseMapping)[text.size()];
            if (!reverseBucket.contains(text))
                reverseBucket.emplace(text, key);
        }
        m_longestSequence = std::max(m_longestSequence, u64(1));
        m_shortestSequence = std::min(m_shortestSequence, u64(1));

        // Prefix-free is sufficient, not necessary; the full test is Sardinas-Patterson.
        if (!m_ambiguousEncoding) {
            std::ranges::sort(encodedValues);
            for (size_t i = 1; i < encodedValues.size(); i++) {
                if (encodedValues[i].starts_with(encodedValues[i - 1])) {
                    m_ambiguousEncoding = true;
                    break;
                }
            }
        }
    }


    const EncodingFile* getEncodingByName(const std::string &name) {
        static std::mutex mutex;
        static std::map<std::string, EncodingFile> encodings;

        std::scoped_lock lock(mutex);

        if (const auto entry = encodings.find(name); entry != encodings.end())
            return entry->second.valid() ? &entry->second : nullptr;

        // An encoding name is conventionally case-insensitive.
        const auto lowerCaseName = toLower(name);

        auto path = findEncodingFile(name);

        // Rejects a bare file stem when the encoding has a real name in the alias table.
        if (path.has_value()) {
            const bool nameIsBareStem = std::ranges::any_of(EncodingNameAliases, [&](const auto &entry) {
                return entry.second == lowerCaseName;
            }) && std::ranges::none_of(EncodingNameAliases, [&](const auto &entry) {
                return entry.first == lowerCaseName;
            });

            if (nameIsBareStem)
                path.reset();
        }

        if (!path.has_value()) {
            for (const auto &[alias, fileStem] : EncodingNameAliases) {
                if (alias != lowerCaseName)
                    continue;

                path = findEncodingFile(fileStem);
                break;
            }
        }

        EncodingFile encoding;
        if (path.has_value())
            encoding = EncodingFile(EncodingFile::Type::Thingy, *path);

        // A failed lookup is cached too, so a bad name hits the file system once.
        const auto &result = encodings.emplace(name, std::move(encoding)).first->second;
        return result.valid() ? &result : nullptr;
    }

    const Codepage& Codepage::ascii() {
        static const Codepage asciiCodepage = [] {
            Codepage result;

            // A control code has no character to draw, so its entry stays empty.
            for (size_t byte = 0x20; byte < 0x7F; byte += 1)
                result.m_characters[byte] = std::string(1, char(byte));

            return result;
        }();

        return asciiCodepage;
    }

    std::optional<Codepage> Codepage::fromEncoding(const EncodingFile &encoding) {
        if (!encoding.valid() || encoding.getLongestSequence() != 1)
            return std::nullopt;

        Codepage result;
        result.m_name = encoding.getName();

        for (size_t byte = 0; byte <= 0xFF; byte += 1) {
            const auto key = u8(byte);
            if (isControlCode(key))
                continue;

            const auto entry = encoding.lookup(std::span(&key, 1));
            if (!entry.has_value())
                continue;

            if (const auto text = entry->first; isSingleCharacter(text))
                result.m_characters[byte] = text;
        }

        return result;
    }

    pl::core::DecodeResult decodeUtf8Bounded(std::span<const u8> bytes, std::optional<size_t> maxCodepoints) {
        pl::core::DecodeResult result;

        while (!bytes.empty()) {
            if (maxCodepoints.has_value() && result.codepointCount >= *maxCodepoints) {
                result.stopReason = pl::core::DecodeStop::CodepointLimit;
                return result;
            }

            const std::string_view text(reinterpret_cast<const char *>(bytes.data()), bytes.size());
            const auto info = readUtf8Codepoint(text);

            if (info.status == Utf8CodepointStatus::Incomplete) {
                result.stopReason = pl::core::DecodeStop::EndOfInput;
                return result;
            }
            if (info.status == Utf8CodepointStatus::Invalid) {
                result.stopReason = pl::core::DecodeStop::MalformedBytes;
                return result;
            }

            result.text.append(text.substr(0, info.length));
            result.bytesConsumed += info.length;
            result.codepointCount += 1;
            bytes = bytes.subspan(info.length);
        }

        result.stopReason = pl::core::DecodeStop::EndOfInput;
        return result;
    }

    pl::core::DecodeResult decodeUtf16Bounded(std::span<const u8> bytes, std::endian endian, std::optional<size_t> maxCodepoints) {
        pl::core::DecodeResult result;

        while (bytes.size() >= 2) {
            if (maxCodepoints.has_value() && result.codepointCount >= *maxCodepoints) {
                result.stopReason = pl::core::DecodeStop::CodepointLimit;
                return result;
            }

            const u16 unit = readUtf16Unit(bytes.subspan(0, 2), endian);
            const bool isHighSurrogate = unit >= 0xD800 && unit <= 0xDBFF;
            const bool isLowSurrogate  = unit >= 0xDC00 && unit <= 0xDFFF;

            if (isLowSurrogate) {
                result.stopReason = pl::core::DecodeStop::MalformedBytes;
                return result;
            }

            u32 codepoint  = unit;
            size_t advance = 2;

            if (isHighSurrogate) {
                if (bytes.size() < 4) {
                    result.stopReason = pl::core::DecodeStop::EndOfInput;
                    return result;
                }

                const u16 low = readUtf16Unit(bytes.subspan(2, 2), endian);
                if (low < 0xDC00 || low > 0xDFFF) {
                    result.stopReason = pl::core::DecodeStop::MalformedBytes;
                    return result;
                }

                codepoint = 0x10000 + (u32(unit - 0xD800) << 10) + (low - 0xDC00);
                advance   = 4;
            }

            auto utf8 = wolv::util::utf32ToUtf8(std::u32string(1, char32_t(codepoint)));
            if (!utf8.has_value()) {
                result.stopReason = pl::core::DecodeStop::MalformedBytes;
                return result;
            }

            result.text += *utf8;
            result.bytesConsumed += advance;
            result.codepointCount += 1;
            bytes = bytes.subspan(advance);
        }

        result.stopReason = pl::core::DecodeStop::EndOfInput;
        return result;
    }

    pl::core::DecodeResult decodeUtf32Bounded(std::span<const u8> bytes, std::endian endian, std::optional<size_t> maxCodepoints) {
        pl::core::DecodeResult result;

        while (bytes.size() >= 4) {
            if (maxCodepoints.has_value() && result.codepointCount >= *maxCodepoints) {
                result.stopReason = pl::core::DecodeStop::CodepointLimit;
                return result;
            }

            u32 codepoint = u32(bytes[0]) | (u32(bytes[1]) << 8) | (u32(bytes[2]) << 16) | (u32(bytes[3]) << 24);
            if (endian == std::endian::big)
                codepoint = ((codepoint & 0x000000FF) << 24) | ((codepoint & 0x0000FF00) << 8)
                          | ((codepoint & 0x00FF0000) >> 8)  | ((codepoint & 0xFF000000) >> 24);

            const bool valid = codepoint <= 0x10FFFF && !(codepoint >= 0xD800 && codepoint <= 0xDFFF);
            if (!valid) {
                result.stopReason = pl::core::DecodeStop::MalformedBytes;
                return result;
            }

            auto utf8 = wolv::util::utf32ToUtf8(std::u32string(1, char32_t(codepoint)));
            if (!utf8.has_value()) {
                result.stopReason = pl::core::DecodeStop::MalformedBytes;
                return result;
            }

            result.text += *utf8;
            result.bytesConsumed += 4;
            result.codepointCount += 1;
            bytes = bytes.subspan(4);
        }

        result.stopReason = pl::core::DecodeStop::EndOfInput;
        return result;
    }

    std::vector<u8> encodeUtf8(std::string_view text) {
        return { text.begin(), text.end() };
    }

    std::optional<std::vector<u8>> encodeUtf16(std::string_view text, std::endian endian) {
        std::vector<u8> result;

        const auto pushUnit = [&](u16 unit) {
            if (endian == std::endian::big)
                unit = u16((unit << 8) | (unit >> 8));
            result.push_back(u8(unit & 0xFF));
            result.push_back(u8((unit >> 8) & 0xFF));
        };

        while (!text.empty()) {
            const auto [status, length, codepoint] = readUtf8Codepoint(text);
            if (status != Utf8CodepointStatus::Complete)
                return std::nullopt;

            if (codepoint <= 0xFFFF) {
                pushUnit(u16(codepoint));
            } else {
                const u32 value = codepoint - 0x10000;
                pushUnit(u16(0xD800 + (value >> 10)));
                pushUnit(u16(0xDC00 + (value & 0x3FF)));
            }

            text = text.substr(length);
        }

        return result;
    }

    std::optional<std::vector<u8>> encodeUtf32(std::string_view text, std::endian endian) {
        std::vector<u8> result;

        while (!text.empty()) {
            const auto [status, length, codepoint] = readUtf8Codepoint(text);
            if (status != Utf8CodepointStatus::Complete)
                return std::nullopt;

            u32 value = codepoint;
            if (endian == std::endian::big)
                value = ((value & 0x000000FF) << 24) | ((value & 0x0000FF00) << 8)
                      | ((value & 0x00FF0000) >> 8)  | ((value & 0xFF000000) >> 24);

            result.push_back(u8(value & 0xFF));
            result.push_back(u8((value >> 8) & 0xFF));
            result.push_back(u8((value >> 16) & 0xFF));
            result.push_back(u8((value >> 24) & 0xFF));

            text = text.substr(length);
        }

        return result;
    }

    bool isAlgorithmicEncodingName(std::string_view name) {
        return name == "UTF-8"
            || name == "UTF-16LE" || name == "UTF-16BE"
            || name == "UTF-32LE" || name == "UTF-32BE";
    }

    std::optional<pl::core::DecodeResult> decodeAlgorithmicTextBounded(std::string_view name, std::span<const u8> bytes, std::optional<size_t> maxCodepoints) {
        if (name == "UTF-8")
            return decodeUtf8Bounded(bytes, maxCodepoints);

        if (name == "UTF-16LE" || name == "UTF-16BE")
            return decodeUtf16Bounded(bytes, name == "UTF-16LE" ? std::endian::little : std::endian::big, maxCodepoints);

        if (name == "UTF-32LE" || name == "UTF-32BE")
            return decodeUtf32Bounded(bytes, name == "UTF-32LE" ? std::endian::little : std::endian::big, maxCodepoints);

        return std::nullopt;
    }

    /**
     * @brief Checks whether the font draws something for a code point above ASCII
     *
     * Unicode has no "printable" property to ask for, so this lists the General_Category values
     * that draw nothing: Cc, Cf, Zl and Zp. It is a hand-kept subset, not a Unicode database.
     * A code point it misses shows as itself, which is the safe way to be wrong.
     */
    static bool hasGlyphAboveAscii(char32_t codepoint) {
        if (codepoint <= 0x9F) return false;                            // Cc: C1 controls
        if (codepoint == 0xAD) return false;                            // Cf: soft hyphen
        // ZWJ and ZWNJ are Cf, but they shape the text on each side, so they stay visible.
        if (codepoint == 0x200B) return false;                          // Cf: zero width space
        if (codepoint >= 0x200E && codepoint <= 0x200F) return false;   // Cf: LTR and RTL marks
        if (codepoint == 0x2028 || codepoint == 0x2029) return false;   // Zl, Zp: line/paragraph separator
        if (codepoint >= 0x202A && codepoint <= 0x202E) return false;   // Cf: bidi embedding/override
        if (codepoint >= 0x2060 && codepoint <= 0x2064) return false;   // Cf: word joiner, invisible operators
        if (codepoint == 0xFEFF) return false;                          // Cf: BOM / zero width no-break space
        if (codepoint >= 0xFFF9 && codepoint <= 0xFFFB) return false;   // Cf: interlinear annotation
        if (codepoint == 0x110BD || codepoint == 0x110CD) return false; // Cf: Kaithi number signs
        if (codepoint >= 0x13430 && codepoint <= 0x1343F) return false; // Cf: Egyptian format controls
        if (codepoint >= 0x1BCA0 && codepoint <= 0x1BCA3) return false; // Cf: shorthand format controls
        if (codepoint >= 0x1D173 && codepoint <= 0x1D17A) return false; // Cf: musical format controls
        if (codepoint >= 0xE0000 && codepoint <= 0xE007F) return false; // Cf: tags
        return true;
    }

    std::string escapeCodepoint(char32_t codepoint) {
        if (codepoint < 0x80)
            return escapeByte(u8(codepoint));

        if (hasGlyphAboveAscii(codepoint))
            return wolv::util::utf32ToUtf8(std::u32string(1, codepoint)).value_or("?");

        // \uNNNN cannot reach past the Basic Multilingual Plane.
        if (codepoint > 0xFFFF)
            return fmt::format("\\U{:08X}", u32(codepoint));
        return fmt::format("\\u{:04X}", u32(codepoint));
    }

    std::optional<std::string> escapeControlCharacters(std::string_view text) {
        std::string result;

        for (size_t offset = 0; offset < text.size();) {
            const auto [status, length, codepoint] = readUtf8Codepoint(text.substr(offset));
            if (status != Utf8CodepointStatus::Complete) {
                // A bad byte has no character to escape; the caller shows "Invalid".
                return std::nullopt;
            }

            result += escapeCodepoint(codepoint);
            offset += length;
        }

        return result;
    }

}
