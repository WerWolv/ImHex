#include <hex/helpers/string_codec.hpp>
#include <hex/helpers/encoding_file.hpp>
#include <hex/helpers/unicode.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>

#include <algorithm>
#include <span>

namespace hex {

    namespace {

        /**
         * @brief Resolves to `encoding`, else the document's declared encoding, else UTF-8
         */
        std::string resolveEncodingName(std::string_view encoding) {
            if (!encoding.empty())
                return std::string(encoding);

            if (const auto declaredEncoding = ImHexApi::HexEditor::getEncodingName(); declaredEncoding.has_value())
                return *declaredEncoding;

            return "UTF-8";
        }

        /**
         * @brief Encodes text under the algorithmic Unicode encoding `name` names
         */
        std::optional<std::vector<u8>> encodeAlgorithmicText(std::string_view name, std::string_view text) {
            // Checked once here. encodeUtf8() trusts its input and would copy a bad sequence.
            if (!isValidUtf8(text))
                return std::nullopt;

            if (name == "UTF-8")
                return encodeUtf8(text);
            if (name == "UTF-16LE")
                return encodeUtf16(text, std::endian::little);
            if (name == "UTF-16BE")
                return encodeUtf16(text, std::endian::big);
            if (name == "UTF-32LE")
                return encodeUtf32(text, std::endian::little);
            if (name == "UTF-32BE")
                return encodeUtf32(text, std::endian::big);

            return std::nullopt;
        }

        /**
         * @brief Replaces each malformed or truncated UTF-8 sequence with U+FFFD
         */
        std::string sanitizeUtf8(std::string_view text) {
            std::string result;

            while (!text.empty()) {
                const auto step = decodeUtf8Bounded(std::span(reinterpret_cast<const u8 *>(text.data()), text.size()), 1);
                if (step.bytesConsumed == 0) {
                    result += "\xEF\xBF\xBD";
                    text = text.substr(1);
                    continue;
                }

                result += text.substr(0, step.bytesConsumed);
                text = text.substr(step.bytesConsumed);
            }

            return result;
        }

    }

    pl::core::DecodeResult PatternLanguageStringCodec::decode(std::span<const u8> bytes, std::string_view encoding, std::optional<size_t> maxCodepoints) const {
        const auto name = resolveEncodingName(encoding);

        if (const auto algorithmic = decodeAlgorithmicTextBounded(name, bytes, maxCodepoints); algorithmic.has_value())
            return *algorithmic;

        const auto *table = getEncodingByName(name);
        if (table == nullptr) {
            pl::core::DecodeResult result;
            result.stopReason = pl::core::DecodeStop::MalformedBytes;
            return result;
        }

        return table->decodeBounded(bytes, maxCodepoints);
    }

    std::optional<std::vector<u8>> PatternLanguageStringCodec::encode(std::string_view text, std::string_view encoding) const {
        const auto name = resolveEncodingName(encoding);

        // Its own encoder answers alone. A fallthrough would find a .tbl with the same name.
        if (isAlgorithmicEncodingName(name))
            return encodeAlgorithmicText(name, text);

        const auto *table = getEncodingByName(name);
        if (table == nullptr)
            return std::nullopt;

        return table->encodeAll(text);
    }

    std::vector<u8> PatternLanguageStringCodec::encodeLossy(std::string_view text, std::string_view encoding) const {
        const auto name = resolveEncodingName(encoding);

        // Every path below needs well-formed UTF-8, so do this once.
        const std::string sanitized = sanitizeUtf8(text);

        // Sanitized text is valid UTF-8, so an algorithmic encoding always has an answer.
        if (isAlgorithmicEncodingName(name))
            return encodeAlgorithmicText(name, sanitized).value_or(std::vector<u8>{});

        const auto *table = getEncodingByName(name);
        if (table == nullptr)
            return {};

        // U+FFFD when the encoding has a byte for it. Most single byte codepages do not.
        const auto replacement = table->getBytesFor("\xEF\xBF\xBD").value_or(table->getBytesFor("?").value_or(std::pair<std::vector<u8>, size_t>{}));

        // Not encodeAll(), which refuses an ambiguous table. A lossy write accepts approximation.
        std::string_view remaining = sanitized;

        std::vector<u8> result;
        while (!remaining.empty()) {
            if (auto match = table->getBytesFor(remaining); match.has_value()) {
                result.insert(result.end(), match->first.begin(), match->first.end());
                remaining = remaining.substr(match->second);
                continue;
            }

            result.insert(result.end(), replacement.first.begin(), replacement.first.end());

            const auto step = decodeUtf8Bounded(std::span(reinterpret_cast<const u8 *>(remaining.data()), remaining.size()), 1);
            remaining = remaining.substr(std::min(step.bytesConsumed, remaining.size()));
        }

        return result;
    }

}
