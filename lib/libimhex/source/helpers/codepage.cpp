#include <hex/helpers/codepage.hpp>

#include <hex/helpers/encoding_file.hpp>
#include <hex/helpers/unicode.hpp>

#include <span>

namespace hex {

    const Codepage& Codepage::ascii() {
        static const Codepage asciiCodepage = [] {
            Codepage result;

            for (size_t byte = 0; byte <= 0x7F; byte += 1)
                result.m_characters[byte] = Codepoint(byte);

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
            const auto entry = encoding.lookup(std::span(&key, 1));
            if (!entry.has_value())
                continue;

            // A byte that draws as more than one code point does not fit in a cell.
            const auto codepoint = decodeSingleCodepoint(entry->first);
            if (!codepoint.has_value())
                return std::nullopt;

            result.m_characters[byte] = Codepoint(*codepoint);
        }

        return result;
    }

}
