#pragma once

#include <hex.hpp>

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

    /**
     * @brief Reads the table a `-include` line names
     * @param name The included table's name, without the extension
     * @return The table's content, or std::nullopt when there is no such table
     */
    using IncludeResolver = std::function<std::optional<std::string>(std::string_view name)>;

    /**
     * @brief A byte sequence to text table
     *
     * One line gives one entry, as "HEX BYTES=text". A header of directives comes above the
     * first entry. A line that starts with "-" below it makes the table invalid:
     *
     * - `-name text` gives the encoding its name, with the case it is written with. Name the
     *   file encodingFileName() of it, since a name reaches a table through its file's name.
     * - `-include name` brings in the entries of the encodings/name.tbl table. It fills only
     *   the bytes this table gives no value of its own, whatever order the lines are in.
     * - `-description text` says what the table is for. It takes the rest of the line.
     * - `-alias name` makes this table another name for encodings/name.tbl, the way a symbolic
     *   link is another name for a file. Such a table holds nothing else but comments, which
     *   are the lines the parser cannot read.
     *
     * An include carries only entries. A table's name and description are its own.
     */
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

        /**
         * @brief Parses a table from text
         * @param type The table's format
         * @param content The table's text
         * @param resolveInclude Reads a table that a `-include` line names. Empty reads from the
         * encodings directory, which is what a table file on disk needs.
         */
        EncodingFile(Type type, const std::string &content, IncludeResolver resolveInclude = {});

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

        /**
         * @brief Gets the table's text, with every `-include` line already replaced
         * @return The text, which needs no other table to parse again
         */
        [[nodiscard]] const std::string& getTableContent() const { return m_tableContent; }

        /**
         * @brief Gets the encoding's name
         * @return Its `-name` line, or a name made from the file's own name when it has none
         */
        [[nodiscard]] const std::string& getName() const { return m_name; }

        /**
         * @brief Gets what the table says it is for
         * @return Its `-description` line, or an empty string when it has none
         */
        [[nodiscard]] const std::string& getDescription() const { return m_description; }

    private:
        /**
         * @brief Reads the table's text into this encoding
         * @return False when a directive comes below the first entry, or a `-alias` line sits in
         * a table with contents of its own, or links to a table that is missing, or loops
         */
        bool parse(const std::string &content, const IncludeResolver &resolveInclude);

        bool m_valid = false;

        std::string m_name;
        std::string m_description;
        std::string m_tableContent;
        std::unique_ptr<std::map<size_t, std::map<std::vector<u8>, std::string>>> m_mapping;
        std::unique_ptr<std::map<size_t, std::map<std::string, std::vector<u8>, std::less<>>>> m_reverseMapping;

        u64 m_shortestSequence = std::numeric_limits<u64>::max();
        u64 m_longestSequence  = std::numeric_limits<u64>::min();

        bool m_ambiguousEncoding = false;
    };

    /**
     * @brief Looks an encoding up by its name
     *
     * `name` reaches a file through encodingFileName(), which keeps the lookup in the encodings
     * directory. A table names itself with a `-name` line, so `#pragma encoding` finds it by
     * that name. A table whose only line is `-alias` is another name for the table it points
     * at. Each table is parsed once and cached for the life of the process.
     *
     * @param name The encoding's name
     * @return The encoding, or nullptr when no such table exists
     */
    const EncodingFile* getEncodingByName(const std::string &name);

    /**
     * @brief Makes the name of the file a table with this name lives in
     *
     * A file system does not carry every character a name has, and does not always tell case
     * apart. So "Windows-1252" lives in windows_1252.tbl.
     *
     * @param name The encoding's name
     * @return The name in lower case, with "_" for every character that is not a letter or a digit
     */
    std::string encodingFileName(std::string_view name);

    /**
     * @brief What a table's header says about the encoding
     */
    struct EncodingHeader {
        std::string name;
        std::string description;

        // True when a `-alias` line led here, so this file is only another name for the table.
        bool isAlias = false;

        // The table the header came from, which a `-alias` line makes a different file. Empty
        // when a link breaks or loops, and so reaches no table at all.
        std::fs::path path;
    };

    /**
     * @brief Reads the header of a table file, and no more of it
     *
     * Every directive comes above the first entry, so this reads only the start of the file. A
     * table with many thousands of entries costs no more than a small one. Follows a `-alias`
     * line to the table it links to.
     *
     * @param path The table file to read
     * @return What its header says, with a name made from the file's name when it gives none
     */
    EncodingHeader readEncodingHeader(const std::fs::path &path);

}
