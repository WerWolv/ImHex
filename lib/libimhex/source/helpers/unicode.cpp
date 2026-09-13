#include <hex/helpers/unicode.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils.hpp>

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
