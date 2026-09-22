#pragma once

#include <hex.hpp>

#include <array>
#include <optional>
#include <string>

namespace hex {

    class EncodingFile;

    /**
     * @brief A Unicode code point
     *
     * Invalid stands for no code point. Unicode stops at U+10FFFF, so -1 is never a character.
     */
    enum class Codepoint : char32_t {
        Invalid = char32_t(-1)
    };

    /**
     * @brief A single-byte character set
     *
     * One byte maps to at most one character, for all 256 byte values. Has no state and no
     * multi-byte sequences, so a text view can start decoding at any scroll position and give
     * every byte its own cell.
     */
    class Codepage {
    public:
        /**
         * @brief The plain 7-bit ASCII codepage
         * @return The codepage a file's text reads with when nothing else is declared
         */
        static const Codepage& ascii();

        /**
         * @brief Builds a codepage from a single-byte encoding
         * @param encoding The encoding to convert
         * @return The codepage, or std::nullopt when `encoding` is not one: at least one of its
         * characters takes more than one byte, or is more than one code point
         */
        static std::optional<Codepage> fromEncoding(const EncodingFile &encoding);

        /**
         * @brief Looks up the character a byte stands for
         * @param byte The byte to look up
         * @return Its code point, which can be a control code, or Codepoint::Invalid when the
         * codepage maps the byte to no character at all
         */
        [[nodiscard]] Codepoint operator[](u8 byte) const { return m_characters[byte]; }

        /**
         * @brief Gets the codepage's name
         * @return The name, which is empty for the default ASCII codepage
         */
        [[nodiscard]] const std::string& getName() const { return m_name; }

    private:
        Codepage() { m_characters.fill(Codepoint::Invalid); }

        std::array<Codepoint, 256> m_characters;
        std::string m_name;
    };

}
