#pragma once

#include <hex.hpp>

#include <array>
#include <bit>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <span>

#include <wolv/io/fs.hpp>

#include <pl/core/string_encode_decode.hpp>

namespace hex {

    namespace impl {
        void appendEncodingLineStartAddress(std::vector<u64> &lineStartAddresses, size_t line, u64 nextLineStartAddress);
    }

    class EncodingFile {
    public:
        enum class Type
        {
            Thingy
        };

        EncodingFile();
        EncodingFile(const EncodingFile &other);
        EncodingFile(EncodingFile &&other) noexcept;
        EncodingFile(Type type, const std::fs::path &path);
        EncodingFile(Type type, const std::string &content);

        EncodingFile& operator=(const EncodingFile &other);
        EncodingFile& operator=(EncodingFile &&other) noexcept;

        /**
         * @brief Decodes the longest byte sequence `buffer` starts with
         * @param buffer The bytes to decode
         * @return The decoded text and the number of bytes it used, or std::nullopt when this
         * encoding has no entry for these bytes
         */
        [[nodiscard]] std::optional<std::pair<std::string_view, size_t>> lookup(std::span<const u8> buffer) const;

        [[nodiscard]] std::pair<std::string_view, size_t> getEncodingFor(std::span<const u8> buffer) const;
        [[nodiscard]] u64 getEncodingLengthFor(std::span<u8> buffer) const;
        [[nodiscard]] u64 getShortestSequence() const { return m_shortestSequence; }
        [[nodiscard]] u64 getLongestSequence()  const { return m_longestSequence;  }
        [[nodiscard]] std::string decodeAll(std::span<const u8> buffer) const;

        /**
         * @brief Checks whether every byte of `buffer` decodes to a known value
         *
         * False for a buffer getEncodingFor() would otherwise paper over with a "." placeholder.
         *
         * @param buffer The bytes to check
         * @return Whether the whole buffer decodes with no unmapped byte left over
         */
        [[nodiscard]] bool isFullyMapped(std::span<const u8> buffer) const;

        /**
         * @brief Decodes `buffer` one entry at a time, up to a limit
         *
         * Unlike isFullyMapped() with decodeAll(), this tells a buffer too short for even the
         * shortest mapped sequence (DecodeStop::EndOfInput) apart from one holding a byte
         * sequence this encoding does not know (DecodeStop::MalformedBytes).
         *
         * @param buffer The bytes to decode
         * @param maxCodepoints The most entries to decode, or std::nullopt for no limit
         * @return The decoded text, the bytes it used, and why decoding stopped
         */
        [[nodiscard]] pl::core::DecodeResult decodeBounded(std::span<const u8> buffer, std::optional<size_t> maxCodepoints = std::nullopt) const;

        /**
         * @brief Encodes the longest decoded value `sequence` starts with
         * @param sequence The text to encode
         * @return The encoded bytes and the number of characters they came from, or std::nullopt
         * when `sequence` does not start with a known decoded value
         */
        [[nodiscard]] std::optional<std::pair<std::vector<u8>, size_t>> getBytesFor(std::string_view sequence) const;

        /**
         * @brief Checks whether this encoding can encode as well as decode
         * @return False when one decoded value maps to more than one byte sequence, or one
         * decoded value is a prefix of another
         */
        [[nodiscard]] bool canEncode() const { return !m_ambiguousEncoding; }

        /**
         * @brief Encodes the whole of `sequence`
         * @param sequence The text to encode
         * @return The encoded bytes, or std::nullopt when the encoding is ambiguous (see
         * canEncode()) or `sequence` has a character with no byte value in it
         */
        [[nodiscard]] std::optional<std::vector<u8>> encodeAll(std::string_view sequence) const;

        [[nodiscard]] bool valid() const { return m_valid; }

        [[nodiscard]] const std::string& getTableContent() const { return m_tableContent; }

        [[nodiscard]] const std::string& getName() const { return m_name; }

    private:
        void parse(const std::string &content);

        bool m_valid = false;

        std::string m_name;
        std::string m_tableContent;
        std::unique_ptr<std::map<size_t, std::map<std::vector<u8>, std::string>>> m_mapping;
        std::unique_ptr<std::map<size_t, std::map<std::string, std::vector<u8>, std::less<>>>> m_reverseMapping;

        u64 m_shortestSequence = std::numeric_limits<u64>::max();
        u64 m_longestSequence  = std::numeric_limits<u64>::min();

        bool m_ambiguousEncoding = false;
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
         * characters takes more than one byte
         */
        static std::optional<Codepage> fromEncoding(const EncodingFile &encoding);

        /**
         * @brief Looks up the character a byte draws as
         * @param byte The byte to look up
         * @return Its character, or an empty view when the codepage gives it none of its own.
         * That covers both a control code and an unmapped byte.
         */
        [[nodiscard]] std::string_view operator[](u8 byte) const { return m_characters[byte]; }

        /**
         * @brief Gets the codepage's name
         * @return The name, which is empty for the default ASCII codepage
         */
        [[nodiscard]] const std::string& getName() const { return m_name; }

    private:
        Codepage() = default;

        std::array<std::string, 256> m_characters;
        std::string m_name;
    };

    /**
     * @brief Looks an encoding up by its table file's name, without the extension
     *
     * For example, "macintosh" finds encodings/macintosh.tbl. Looks only in the configured
     * encodings directory and discards any directory part in `name`. Each table is parsed once
     * and cached for the life of the process.
     *
     * @param name The table file's name
     * @return The encoding, or nullptr when no such table exists
     */
    const EncodingFile* getEncodingByName(const std::string &name);

}
