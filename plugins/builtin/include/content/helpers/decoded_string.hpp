#pragma once

#include <hex.hpp>

#include <ui/control_byte_picture.hpp>

#include <pl/core/string_encode_decode.hpp>

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hex::plugin::builtin {

    using hex::ui::NulPicture;
    using hex::ui::nulToPicture;
    using hex::ui::pictureToNul;

    /**
     * @brief The most code points a Data Inspector string row decodes for display
     *
     * Matches PatternString::formatDisplayValue()'s own budget, so a hover
     * tooltip and the tree view agree on where to mark a string truncated.
     */
    constexpr static auto DisplayBudget = 0x7F;

    /**
     * @brief Extends a selection forward to the end of the code point it lands in
     *
     * A click-select size must never end inside a code point cell.
     *
     * @param buffer The bytes to decode from
     * @param targetSize The selection size to extend, in bytes
     * @param decodeOne Decodes one code point from the front of a span
     * @return The extended size; past `targetSize` only when a code point straddles it
     */
    size_t extendToWholeCodePoints(const std::vector<u8> &buffer, size_t targetSize,
        const std::function<pl::core::DecodeResult(std::span<const u8>)> &decodeOne);

    /**
     * @brief Decodes a string until it covers a selection, or a display limit
     *
     * A malformed code point before `targetSize` invalidates the whole result
     * (stopReason MalformedBytes); one after just ends the display window,
     * the same as PatternString's own budget cutoff.
     *
     * @param buffer The bytes to decode from
     * @param targetSize The selection size to cover, in bytes
     * @param codepointLimit The most code points to decode for display
     * @param decodeOne Decodes one code point from the front of a span
     * @return The decoded text
     */
    pl::core::DecodeResult decodeThroughSelection(const std::vector<u8> &buffer, size_t targetSize, size_t codepointLimit,
        const std::function<pl::core::DecodeResult(std::span<const u8>)> &decodeOne);

    /**
     * @brief Formats the one code point at a buffer's start under a named algorithmic encoding
     * @param encodingName The algorithmic encoding to decode under
     * @param buffer The bytes to decode from
     * @return The character and its U+ notation, or nullopt if those bytes are not one whole, valid code point
     */
    std::optional<std::string> formatCodePoint(std::string_view encodingName, std::span<const u8> buffer);

    /**
     * @brief The byte size of the code point at a buffer's start
     * @param encodingName The algorithmic encoding to decode under
     * @param buffer The bytes to decode from
     * @param codeUnitSize The size to fall back to when decoding fails
     * @return The code point's byte size, or `codeUnitSize` so a malformed row still selects one whole unit
     */
    size_t codePointSize(std::string_view encodingName, std::span<const u8> buffer, size_t codeUnitSize);

    /**
     * @brief Formats a decoded string row like PatternString::formatDisplayValue()
     * @param literalPrefix The C++ string literal prefix for the row's encoding
     * @param decoded The decoded text
     * @param selectionSize The selection size, in bytes
     * @return The quoted, escaped string, marked truncated if the selection held more
     */
    std::string formatDecodedString(std::string_view literalPrefix, const pl::core::DecodeResult &decoded, size_t selectionSize);

}
