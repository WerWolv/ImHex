#pragma once

#include <hex.hpp>

#include <bit>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <pl/core/string_encode_decode.hpp>

namespace hex {

    /**
     * @brief Checks whether `text` is exactly one whole, valid UTF-8 code point
     * @param text The text to check
     * @return Whether the text is one code point, and so fits in a single cell
     */
    bool isSingleCharacter(std::string_view text);

    /**
     * @brief Reads `text` as exactly one whole, valid UTF-8 code point
     * @param text The text to read
     * @return Its code point, or std::nullopt when isSingleCharacter() is false for it
     */
    std::optional<char32_t> decodeSingleCodepoint(std::string_view text);

    /**
     * @brief Checks whether `codepoint` is a C0 control or DELETE
     * @param codepoint The code point to check
     * @return Whether it is a control code, which has no glyph. C1, U+0080 to U+009F, is not
     * one, since Unicode has a picture character for only C0 and DELETE.
     */
    bool isControlCode(char32_t codepoint);

    /**
     * @brief Escapes one decoded code point for display
     *
     * A non-printable ASCII byte becomes \xNN. A code point above ASCII with no glyph becomes
     * \uNNNN, or \UNNNNNNNN past U+FFFF. Everything else passes through.
     *
     * @param codepoint The code point to escape
     * @return The text to display for it
     */
    std::string escapeCodepoint(char32_t codepoint);

    /**
     * @brief Escapes whole decoded UTF-8 text with escapeCodepoint()
     * @param text The text to escape
     * @return The escaped text, or std::nullopt on invalid UTF-8, since a bad byte cannot
     * round-trip as an escape. Show "Invalid" instead.
     */
    std::optional<std::string> escapeControlCharacters(std::string_view text);

    /**
     * @brief Checks whether `text` is well-formed UTF-8 all the way through
     * @param text The text to check
     * @return Whether every byte of it belongs to a valid code point
     */
    bool isValidUtf8(std::string_view text);

    /**
     * @brief Encodes UTF-8 text as UTF-8 bytes
     * @param text The text to encode
     * @return The encoded bytes. This never fails; the input is already UTF-8.
     */
    std::vector<u8> encodeUtf8(std::string_view text);

    /**
     * @brief Encodes UTF-8 text as UTF-16 bytes
     * @param text The text to encode
     * @param endian The byte order to write the code units in
     * @return The encoded bytes, or std::nullopt when `text` is not valid UTF-8
     */
    std::optional<std::vector<u8>> encodeUtf16(std::string_view text, std::endian endian);

    /**
     * @brief Encodes UTF-8 text as UTF-32 bytes
     * @param text The text to encode
     * @param endian The byte order to write the code units in
     * @return The encoded bytes, or std::nullopt when `text` is not valid UTF-8
     */
    std::optional<std::vector<u8>> encodeUtf32(std::string_view text, std::endian endian);

    /**
     * @brief Checks whether `name` names an algorithmic Unicode encoding
     *
     * These are "UTF-8", "UTF-16LE", "UTF-16BE", "UTF-32LE" and "UTF-32BE", spelled the way
     * ICU's converter names spell them. A name this rejects is a .tbl table's name; resolve it
     * through getEncodingByName() instead.
     *
     * A bare "UTF-16" or "UTF-32" is deliberately absent. Those name the BOM-carrying encoding
     * schemes of the Unicode Standard, and nothing here reads or writes a BOM, so accepting them
     * would silently guess the byte order.
     *
     * @param name The encoding name to check
     * @return Whether a fixed algorithm decodes it, rather than a table
     */
    bool isAlgorithmicEncodingName(std::string_view name);

    /**
     * @brief Decodes UTF-8 bytes, up to a limit
     *
     * Stops at the first malformed byte, at the end of `bytes`, or after maxCodepoints code
     * points, whichever comes first. A code point cut off by the end of `bytes` stops at
     * DecodeStop::EndOfInput, not DecodeStop::MalformedBytes.
     *
     * @param bytes The bytes to decode
     * @param maxCodepoints The most code points to decode, or std::nullopt for no limit
     * @return The decoded text, the bytes it used, and why decoding stopped
     */
    pl::core::DecodeResult decodeUtf8Bounded(std::span<const u8> bytes, std::optional<size_t> maxCodepoints = std::nullopt);

    /**
     * @brief Decodes UTF-16 bytes, up to a limit. See decodeUtf8Bounded().
     * @param bytes The bytes to decode
     * @param endian The byte order to read the code units in
     * @param maxCodepoints The most code points to decode, or std::nullopt for no limit
     * @return The decoded text, the bytes it used, and why decoding stopped
     */
    pl::core::DecodeResult decodeUtf16Bounded(std::span<const u8> bytes, std::endian endian, std::optional<size_t> maxCodepoints = std::nullopt);

    /**
     * @brief Decodes UTF-32 bytes, up to a limit. See decodeUtf8Bounded().
     * @param bytes The bytes to decode
     * @param endian The byte order to read the code units in
     * @param maxCodepoints The most code points to decode, or std::nullopt for no limit
     * @return The decoded text, the bytes it used, and why decoding stopped
     */
    pl::core::DecodeResult decodeUtf32Bounded(std::span<const u8> bytes, std::endian endian, std::optional<size_t> maxCodepoints = std::nullopt);

    /**
     * @brief Dispatches to the decoder isAlgorithmicEncodingName() accepts `name` for
     * @param name The encoding name
     * @param bytes The bytes to decode
     * @param maxCodepoints The most code points to decode, or std::nullopt for no limit
     * @return The decode result, or std::nullopt for a name no fixed algorithm handles
     */
    std::optional<pl::core::DecodeResult> decodeAlgorithmicTextBounded(std::string_view name, std::span<const u8> bytes, std::optional<size_t> maxCodepoints = std::nullopt);

}
