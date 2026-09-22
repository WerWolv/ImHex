#include "content/helpers/decoded_string.hpp"

#include <hex/api/localization_manager.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/unicode.hpp>

#include <wolv/utils/string.hpp>

namespace hex::plugin::builtin {

    size_t extendToWholeCodePoints(const std::vector<u8> &buffer, size_t targetSize,
            const std::function<pl::core::DecodeResult(std::span<const u8>)> &decodeOne) {
        size_t bytesConsumed = 0;
        while (bytesConsumed < targetSize && bytesConsumed < buffer.size()) {
            const auto result = decodeOne(std::span(buffer).subspan(bytesConsumed));
            if (result.codepointCount == 0)
                break;

            bytesConsumed += result.bytesConsumed;
        }

        return bytesConsumed;
    }

    pl::core::DecodeResult decodeThroughSelection(const std::vector<u8> &buffer, size_t targetSize, size_t codepointLimit,
            const std::function<pl::core::DecodeResult(std::span<const u8>)> &decodeOne) {
        pl::core::DecodeResult total;

        while (total.codepointCount < codepointLimit && total.bytesConsumed < buffer.size()) {
            const auto step = decodeOne(std::span(buffer).subspan(total.bytesConsumed));
            if (step.codepointCount == 0) {
                total.stopReason = (total.bytesConsumed < targetSize) ? pl::core::DecodeStop::MalformedBytes : step.stopReason;
                return total;
            }

            total.text += step.text;
            total.bytesConsumed += step.bytesConsumed;
            total.codepointCount += step.codepointCount;

            if (total.bytesConsumed >= targetSize)
                break;
        }

        return total;
    }

    std::optional<std::string> formatCodePoint(std::string_view encodingName, std::span<const u8> buffer) {
        const auto decoded = decodeAlgorithmicTextBounded(encodingName, buffer, 1);
        if (!decoded.has_value() || decoded->codepointCount == 0)
            return std::nullopt;

        const auto codepoints = wolv::util::utf8ToUtf32(decoded->text);
        if (!codepoints.has_value() || codepoints->empty())
            return std::nullopt;

        const char32_t codepoint = codepoints->front();
        return fmt::format("'{0}' (U+{1:04X})", escapeCodepoint(codepoint), u32(codepoint));
    }

    size_t codePointSize(std::string_view encodingName, std::span<const u8> buffer, size_t codeUnitSize) {
        const auto decoded = decodeAlgorithmicTextBounded(encodingName, buffer, 1);
        if (!decoded.has_value() || decoded->bytesConsumed == 0)
            return codeUnitSize;

        return decoded->bytesConsumed;
    }

    std::string formatDecodedString(std::string_view literalPrefix, const pl::core::DecodeResult &decoded, size_t selectionSize) {
        // Always valid UTF-8, so escapeControlCharacters() never falls back here.
        const auto escaped = escapeControlCharacters(decoded.text).value_or("hex.builtin.inspector.invalid"_lang.get());

        if (decoded.bytesConsumed < selectionSize)
            return fmt::format("{0}\"{1}\" {2}", literalPrefix, escaped, "hex.builtin.inspector.truncated"_lang);

        return fmt::format("{0}\"{1}\"", literalPrefix, escaped);
    }

}
