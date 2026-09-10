#include <hex/helpers/codepage.hpp>

#include <hex/helpers/encoding_file.hpp>
#include <hex/helpers/unicode.hpp>

#include <span>

namespace hex {

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

}
