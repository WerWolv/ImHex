#include <hex/test/tests.hpp>
#include <hex/helpers/unicode.hpp>

#include <content/helpers/decoded_string.hpp>

#include <span>
#include <vector>

using namespace hex::plugin::builtin;

namespace {

    std::vector<u8> bytesOf(std::string_view text) {
        return { text.begin(), text.end() };
    }

    // One UTF-8 code point at a time, the same shape PatternLanguageStringCodec::decode()
    // gives these helpers at run time.
    pl::core::DecodeResult decodeOneUtf8(std::span<const u8> bytes) {
        return hex::decodeUtf8Bounded(bytes, 1);
    }

}

TEST_SEQUENCE("DecodeThroughSelectionStopsAtTheSelection") {
    // The buffer a Data Inspector row receives is read from the selection's
    // start for up to maxSize bytes, whatever the selection length. Decoding to
    // the end of it would read unrelated, unselected file bytes into the row.
    const auto buffer = bytesOf("ab" "UNSELECTED");

    const auto decoded = decodeThroughSelection(buffer, 2, DisplayBudget, decodeOneUtf8);
    TEST_ASSERT(decoded.text == "ab");
    TEST_ASSERT(decoded.bytesConsumed == 2);

    // A one byte selection still stops after one byte.
    const auto single = decodeThroughSelection(buffer, 1, DisplayBudget, decodeOneUtf8);
    TEST_ASSERT(single.text == "a");
    TEST_ASSERT(single.bytesConsumed == 1);

    TEST_SUCCESS();
};

TEST_SEQUENCE("DecodeThroughSelectionFinishesAStraddlingCodePoint") {
    // U+00E9 is two bytes. A selection covering only its first byte still
    // decodes the whole character rather than reporting a broken one.
    const auto buffer = bytesOf("\xC3\xA9" "tail");

    const auto decoded = decodeThroughSelection(buffer, 1, DisplayBudget, decodeOneUtf8);
    TEST_ASSERT(decoded.text == "\xC3\xA9");
    TEST_ASSERT(decoded.bytesConsumed == 2);
    TEST_ASSERT(decoded.stopReason != pl::core::DecodeStop::MalformedBytes);

    TEST_SUCCESS();
};

TEST_SEQUENCE("DecodeThroughSelectionReportsMalformedBytes") {
    // A byte the encoding cannot decode, inside the selection, invalidates the
    // whole row.
    const auto broken = bytesOf("a\xFF" "bc");
    const auto insideSelection = decodeThroughSelection(broken, 4, DisplayBudget, decodeOneUtf8);
    TEST_ASSERT(insideSelection.stopReason == pl::core::DecodeStop::MalformedBytes);

    // The same byte past the selection is just where the display window ends.
    const auto pastSelection = decodeThroughSelection(broken, 1, DisplayBudget, decodeOneUtf8);
    TEST_ASSERT(pastSelection.stopReason != pl::core::DecodeStop::MalformedBytes);
    TEST_ASSERT(pastSelection.text == "a");

    TEST_SUCCESS();
};

TEST_SEQUENCE("DecodeThroughSelectionHonoursTheDisplayBudget") {
    const auto buffer = bytesOf("abcdef");

    const auto decoded = decodeThroughSelection(buffer, 6, 3, decodeOneUtf8);
    TEST_ASSERT(decoded.text == "abc");
    TEST_ASSERT(decoded.codepointCount == 3);

    // Fewer bytes decoded than the selection holds, so the row reads truncated.
    TEST_ASSERT(decoded.bytesConsumed < 6);

    TEST_SUCCESS();
};

TEST_SEQUENCE("ExtendToWholeCodePoints") {
    // A click never selects half a character.
    const auto buffer = bytesOf("a\xC3\xA9" "b");

    TEST_ASSERT(extendToWholeCodePoints(buffer, 1, decodeOneUtf8) == 1);

    // Two bytes lands inside U+00E9, so the size grows to cover it.
    TEST_ASSERT(extendToWholeCodePoints(buffer, 2, decodeOneUtf8) == 3);
    TEST_ASSERT(extendToWholeCodePoints(buffer, 3, decodeOneUtf8) == 3);
    TEST_ASSERT(extendToWholeCodePoints(buffer, 4, decodeOneUtf8) == 4);

    // Asking for more than the buffer holds stops at the buffer.
    TEST_ASSERT(extendToWholeCodePoints(buffer, 99, decodeOneUtf8) == 4);

    TEST_SUCCESS();
};

TEST_SEQUENCE("FormatDecodedStringMarksTruncation") {
    pl::core::DecodeResult decoded;
    decoded.text = "ab";
    decoded.bytesConsumed = 2;

    // The literal prefix is the one the row's encoding uses: none for UTF-8,
    // "u" for UTF-16, "U" for UTF-32.
    TEST_ASSERT(formatDecodedString("", decoded, 2) == "\"ab\"");
    TEST_ASSERT(formatDecodedString("u", decoded, 2) == "u\"ab\"");

    // More bytes selected than got decoded means the display was cut short.
    TEST_ASSERT(formatDecodedString("", decoded, 5).contains("truncated"));

    // Control characters escape rather than breaking the single display line.
    pl::core::DecodeResult withNewline;
    withNewline.text = "a\nb";
    withNewline.bytesConsumed = 3;
    TEST_ASSERT(formatDecodedString("", withNewline, 3) == "\"a\\nb\"");

    TEST_SUCCESS();
};

TEST_SEQUENCE("NulPictureRoundTrip") {
    // ImGui's InputText is NUL-terminated, so a NUL byte needs a stand-in while
    // a value is being edited.
    const std::string withNul("a\0b", 3);

    TEST_ASSERT(nulToPicture(withNul) == "a\xE2\x90\x80" "b");
    TEST_ASSERT(pictureToNul(nulToPicture(withNul)) == withNul);

    // Text with no NUL passes through untouched.
    TEST_ASSERT(nulToPicture("abc") == "abc");
    TEST_ASSERT(pictureToNul("abc") == "abc");

    TEST_SUCCESS();
};
