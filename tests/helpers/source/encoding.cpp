#include <hex/test/tests.hpp>

#include <hex/helpers/codepage.hpp>
#include <hex/helpers/encoding_file.hpp>
#include <hex/helpers/unicode.hpp>

#include <bit>
#include <string>
#include <vector>

using namespace std::literals::string_literals;

namespace {

    std::vector<u8> bytesOf(std::string_view text) {
        return { text.begin(), text.end() };
    }

}

TEST_SEQUENCE("DecodeUnicodeText") {
    using pl::core::DecodeStop;
    const auto whole = [](const pl::core::DecodeResult &result, size_t byteCount) {
        return result.stopReason == DecodeStop::EndOfInput && result.bytesConsumed == byteCount;
    };
    TEST_ASSERT(hex::isValidUtf8("hello"));
    TEST_ASSERT(hex::isValidUtf8(""));
    TEST_ASSERT(hex::isValidUtf8("\xC3\xA9"));
    TEST_ASSERT(!hex::isValidUtf8("\xC0\xAF"));
    TEST_ASSERT(!hex::isValidUtf8("\xED\xA0\x80"));
    TEST_ASSERT(!hex::isValidUtf8("\xC3"));
    TEST_ASSERT(!hex::isValidUtf8("\x80"));

    const auto utf8 = hex::decodeUtf8Bounded(bytesOf("h\xC3\xA9llo"));
    TEST_ASSERT(whole(utf8, 6));
    TEST_ASSERT(utf8.text == "h\xC3\xA9llo");
    TEST_ASSERT(utf8.codepointCount == 5);
    const auto utf16Le = hex::decodeUtf16Bounded(std::vector<u8>{ 0x41, 0x00 }, std::endian::little);
    TEST_ASSERT(whole(utf16Le, 2) && utf16Le.text == "A");

    const auto utf16Be = hex::decodeUtf16Bounded(std::vector<u8>{ 0x00, 0x41 }, std::endian::big);
    TEST_ASSERT(whole(utf16Be, 2) && utf16Be.text == "A");

    const auto pair = hex::decodeUtf16Bounded(std::vector<u8>{ 0x3D, 0xD8, 0x00, 0xDE }, std::endian::little);
    TEST_ASSERT(whole(pair, 4));
    TEST_ASSERT(pair.text == "\xF0\x9F\x98\x80");
    TEST_ASSERT(pair.codepointCount == 1);

    // An odd trailing byte cannot be whole UTF-16. Nothing consumes it.
    const auto oddByte = hex::decodeUtf16Bounded(std::vector<u8>{ 0x41 }, std::endian::little);
    TEST_ASSERT(oddByte.stopReason == DecodeStop::EndOfInput);
    TEST_ASSERT(oddByte.bytesConsumed == 0);
    const auto utf32 = hex::decodeUtf32Bounded(std::vector<u8>{ 0x41, 0x00, 0x00, 0x00 }, std::endian::little);
    TEST_ASSERT(whole(utf32, 4) && utf32.text == "A");

    const auto surrogate = hex::decodeUtf32Bounded(std::vector<u8>{ 0x00, 0xD8, 0x00, 0x00 }, std::endian::little);
    TEST_ASSERT(surrogate.stopReason == DecodeStop::MalformedBytes);

    // A bare "UTF-16" would guess a byte order, so only the specific names decode.
    TEST_ASSERT(hex::isAlgorithmicEncodingName("UTF-8"));
    TEST_ASSERT(hex::isAlgorithmicEncodingName("UTF-16LE"));
    TEST_ASSERT(hex::isAlgorithmicEncodingName("UTF-32BE"));
    TEST_ASSERT(!hex::isAlgorithmicEncodingName("UTF-16"));
    TEST_ASSERT(!hex::isAlgorithmicEncodingName("UTF-32"));
    TEST_ASSERT(!hex::isAlgorithmicEncodingName("shift_jis"));

    TEST_ASSERT(hex::decodeAlgorithmicTextBounded("UTF-16LE", std::vector<u8>{ 0x41, 0x00 })->text == "A");
    TEST_ASSERT(!hex::decodeAlgorithmicTextBounded("shift_jis", std::vector<u8>{ 0x41 }).has_value());

    TEST_SUCCESS();
};

TEST_SEQUENCE("DecodeUnicodeTextBounded") {
    using pl::core::DecodeStop;
    const auto limited = hex::decodeUtf8Bounded(bytesOf("abc"), 2);
    TEST_ASSERT(limited.stopReason == DecodeStop::CodepointLimit);
    TEST_ASSERT(limited.text == "ab");
    TEST_ASSERT(limited.bytesConsumed == 2);
    TEST_ASSERT(limited.codepointCount == 2);
    const auto whole = hex::decodeUtf8Bounded(bytesOf("abc"));
    TEST_ASSERT(whole.stopReason == DecodeStop::EndOfInput);
    TEST_ASSERT(whole.bytesConsumed == 3);

    // A code point cut off by the buffer end is EndOfInput, not MalformedBytes.
    const auto truncated = hex::decodeUtf8Bounded(std::vector<u8>{ 0x41, 0xC3 });
    TEST_ASSERT(truncated.stopReason == DecodeStop::EndOfInput);
    TEST_ASSERT(truncated.text == "A");
    TEST_ASSERT(truncated.bytesConsumed == 1);
    const auto malformed = hex::decodeUtf8Bounded(std::vector<u8>{ 0x41, 0xFF });
    TEST_ASSERT(malformed.stopReason == DecodeStop::MalformedBytes);
    TEST_ASSERT(malformed.bytesConsumed == 1);

    // A high surrogate with no room is EndOfInput; a lone low half never is.
    const auto cutPair = hex::decodeUtf16Bounded(std::vector<u8>{ 0x3D, 0xD8 }, std::endian::little);
    TEST_ASSERT(cutPair.stopReason == DecodeStop::EndOfInput);

    const auto loneLow = hex::decodeUtf16Bounded(std::vector<u8>{ 0x00, 0xDC }, std::endian::little);
    TEST_ASSERT(loneLow.stopReason == DecodeStop::MalformedBytes);

    const auto badUtf32 = hex::decodeUtf32Bounded(std::vector<u8>{ 0x00, 0x00, 0x11, 0x00 }, std::endian::little);
    TEST_ASSERT(badUtf32.stopReason == DecodeStop::MalformedBytes);

    TEST_SUCCESS();
};

TEST_SEQUENCE("EscapeCodepoints") {
    TEST_ASSERT(hex::escapeCodepoint(U'A') == "A");
    TEST_ASSERT(hex::escapeCodepoint(U'\n') == "\\n");
    TEST_ASSERT(hex::escapeCodepoint(U'\\') == "\\\\");
    TEST_ASSERT(hex::escapeCodepoint(char32_t(0x01)) == "\\x01");

    // Above ASCII, only a codepoint with no visible glyph escapes.
    TEST_ASSERT(hex::escapeCodepoint(char32_t(0x00E9)) == "\xC3\xA9");
    TEST_ASSERT(hex::escapeCodepoint(char32_t(0x0085)) == "\\u0085");  // C1 control
    TEST_ASSERT(hex::escapeCodepoint(char32_t(0x200B)) == "\\u200B");  // zero width space
    TEST_ASSERT(hex::escapeCodepoint(char32_t(0x200E)) == "\\u200E");  // left to right mark
    TEST_ASSERT(hex::escapeCodepoint(char32_t(0x200F)) == "\\u200F");  // right to left mark

    // The joiners pass through; escaping a ZWJ would split one emoji into parts.
    TEST_ASSERT(hex::escapeCodepoint(char32_t(0x200C)) == "\xE2\x80\x8C");  // ZWNJ
    TEST_ASSERT(hex::escapeCodepoint(char32_t(0x200D)) == "\xE2\x80\x8D");  // ZWJ

    // Man + ZWJ + woman + ZWJ + girl is one family emoji, and stays whole.
    const auto family = "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7"s;
    TEST_ASSERT(hex::escapeControlCharacters(family).value() == family);
    TEST_ASSERT(hex::escapeCodepoint(char32_t(0xFEFF)) == "\\uFEFF");  // BOM

    // A combining mark is not a format character, so it stays.
    TEST_ASSERT(hex::escapeCodepoint(char32_t(0xFE0F)) == "\xEF\xB8\x8F");

    // \uNNNN stops at the BMP, so a code point past it needs \UNNNNNNNN.
    TEST_ASSERT(hex::escapeCodepoint(char32_t(0x1F600)) == "\xF0\x9F\x98\x80");
    TEST_ASSERT(hex::escapeCodepoint(char32_t(0xE0001)) == "\\U000E0001");  // language tag
    TEST_ASSERT(hex::escapeControlCharacters("ab\nc").value() == "ab\\nc");

    // Every NUL escapes the same way, wherever it sits.
    TEST_ASSERT(hex::escapeControlCharacters("ab\0"s).value() == "ab\\x00");
    TEST_ASSERT(hex::escapeControlCharacters("a\0b"s).value() == "a\\x00b");
    TEST_ASSERT(!hex::escapeControlCharacters("\xFF").has_value());

    TEST_SUCCESS();
};

TEST_SEQUENCE("EncodingFileTable") {
    // "HEX BYTES=text" per line. A byte with no line of its own has no value.
    const hex::EncodingFile table(hex::EncodingFile::Type::Thingy, std::string(
        "41=A\n"
        "80=\xCE\xB1\n"
        "81=\xCE\xB2\n"
        "8140=\xE3\x81\x82\n"));

    TEST_ASSERT(table.valid());
    TEST_ASSERT(table.getShortestSequence() == 1);
    TEST_ASSERT(table.getLongestSequence() == 2);

    // Decoding prefers the longest matching sequence.
    TEST_ASSERT(table.decodeAll(std::vector<u8>{ 0x80, 0x81 }) == "\xCE\xB1\xCE\xB2");
    TEST_ASSERT(table.decodeAll(std::vector<u8>{ 0x81, 0x40 }) == "\xE3\x81\x82");
    TEST_ASSERT(table.decodeAll(std::vector<u8>{ 0x41 }) == "A");
    TEST_ASSERT(table.isFullyMapped(std::vector<u8>{ 0x80, 0x41 }));

    // The table gives 0x42 no value, so it does not decode.
    TEST_ASSERT(table.decodeAll(std::vector<u8>{ 0x42 }) == ".");
    TEST_ASSERT(!table.isFullyMapped(std::vector<u8>{ 0x42 }));
    TEST_ASSERT(!table.isFullyMapped(std::vector<u8>{ 0x90 }));
    TEST_ASSERT(table.canEncode());
    TEST_ASSERT(table.encodeAll("\xCE\xB1\xCE\xB2").value() == (std::vector<u8>{ 0x80, 0x81 }));
    TEST_ASSERT(table.encodeAll("A").value() == std::vector<u8>{ 0x41 });
    TEST_ASSERT(!table.encodeAll("\xE2\x82\xAC").has_value());
    const auto limited = table.decodeBounded(std::vector<u8>{ 0x80, 0x81 }, 1);
    TEST_ASSERT(limited.stopReason == pl::core::DecodeStop::CodepointLimit);
    TEST_ASSERT(limited.bytesConsumed == 1);

    const auto unmapped = table.decodeBounded(std::vector<u8>{ 0x90 });
    TEST_ASSERT(unmapped.stopReason == pl::core::DecodeStop::MalformedBytes);

    TEST_SUCCESS();
};

TEST_SEQUENCE("EncodingFileCodepointEscapes") {
    // \uXXXX names a character by code point, so a byte can map to one with no glyph.
    const hex::EncodingFile table(hex::EncodingFile::Type::Thingy, std::string(
        "00=\\u0000\n"
        "80=\\u20AC\n"
        "81=\\\\\n"
        "82=\\q\n"
        "83=\\uZZZZ\n"));

    TEST_ASSERT(table.valid());
    TEST_ASSERT(table.decodeAll(std::vector<u8>{ 0x00 }) == std::string(1, '\0'));
    TEST_ASSERT(table.decodeAll(std::vector<u8>{ 0x80 }) == "\xE2\x82\xAC");

    // A doubled backslash is one literal backslash.
    TEST_ASSERT(table.decodeAll(std::vector<u8>{ 0x81 }) == "\\");

    // An unreadable escape stands for itself, so a stray backslash survives.
    TEST_ASSERT(table.decodeAll(std::vector<u8>{ 0x82 }) == "\\q");
    TEST_ASSERT(table.decodeAll(std::vector<u8>{ 0x83 }) == "\\uZZZZ");

    // A control code name is just text now, not a control code.
    const hex::EncodingFile named(hex::EncodingFile::Type::Thingy, std::string("80=NUL\n"));
    TEST_ASSERT(named.decodeAll(std::vector<u8>{ 0x80 }) == "NUL");

    TEST_SUCCESS();
};

TEST_SEQUENCE("EncodingFileIncludes") {
    const auto resolve = [](std::string_view name) -> std::optional<std::string> {
        if (name == "base")
            return std::string("-name Base\n41=A\n80=\xCE\xB1\n");
        if (name == "loop")
            return std::string("-include loop\n42=B\n");

        return std::nullopt;
    };

    // An include fills only the bytes the table gives no value of its own.
    const hex::EncodingFile table(hex::EncodingFile::Type::Thingy, std::string(
        "-include base\n"
        "-include no_such_table\n"
        "41=Z\n"), resolve);

    TEST_ASSERT(table.valid());
    TEST_ASSERT(table.decodeAll(std::vector<u8>{ 0x41 }) == "Z");
    TEST_ASSERT(table.decodeAll(std::vector<u8>{ 0x80 }) == "\xCE\xB1");

    // A line the include loses to gives no value to encode back to, either.
    TEST_ASSERT(!table.encodeAll("A").has_value());

    // An included table's name is its own, so it does not come with it.
    TEST_ASSERT(table.getName() == "Unknown");

    // The expanded text needs no other table to parse again.
    const hex::EncodingFile again(hex::EncodingFile::Type::Thingy, table.getTableContent(), resolve);
    TEST_ASSERT(again.decodeAll(std::vector<u8>{ 0x41, 0x80 }) == "Z\xCE\xB1");

    // A table that includes itself brings nothing a second time.
    const hex::EncodingFile cycle(hex::EncodingFile::Type::Thingy, std::string("-include loop\n"), resolve);
    TEST_ASSERT(cycle.decodeAll(std::vector<u8>{ 0x42 }) == "B");

    // Only the header holds a directive, so an include below the entries is not a table.
    const hex::EncodingFile late(hex::EncodingFile::Type::Thingy, std::string(
        "42=B\n"
        "-include base\n"), resolve);
    TEST_ASSERT(!late.valid());

    TEST_SUCCESS();
};

TEST_SEQUENCE("EncodingFileAliases") {
    const auto resolve = [](std::string_view name) -> std::optional<std::string> {
        if (name == "macintosh")
            return std::string("-name Macintosh\n41=A\n");
        if (name == "roman")
            return std::string("-alias macintosh\n");
        if (name == "loop")
            return std::string("-alias loop\n");

        return std::nullopt;
    };

    // A table whose only line is -alias is another name for the table it points at.
    const hex::EncodingFile link(hex::EncodingFile::Type::Thingy, std::string("-alias macintosh\n"), resolve);
    TEST_ASSERT(link.valid());
    TEST_ASSERT(link.getName() == "Macintosh");
    TEST_ASSERT(link.decodeAll(std::vector<u8>{ 0x41 }) == "A");

    // A link to a link reaches the table at the end of the chain.
    const hex::EncodingFile chain(hex::EncodingFile::Type::Thingy, std::string("-alias roman\n"), resolve);
    TEST_ASSERT(chain.getName() == "Macintosh");

    // A link with nothing at the end of it, or one that loops, is not an encoding.
    const hex::EncodingFile broken(hex::EncodingFile::Type::Thingy, std::string("-alias no_such_table\n"), resolve);
    TEST_ASSERT(!broken.valid());

    const hex::EncodingFile loop(hex::EncodingFile::Type::Thingy, std::string("-alias loop\n"), resolve);
    TEST_ASSERT(!loop.valid());

    // Only the header holds a directive, so an alias below the entries is not a table.
    const hex::EncodingFile late(hex::EncodingFile::Type::Thingy, std::string(
        "42=B\n"
        "-alias macintosh\n"), resolve);
    TEST_ASSERT(!late.valid());

    // A line the parser cannot read is a comment, which a link may hold.
    const hex::EncodingFile commented(hex::EncodingFile::Type::Thingy, std::string(
        "Mac OS Roman, under the name ImHex knows it by\n"
        "-alias macintosh\n"), resolve);
    TEST_ASSERT(commented.valid());
    TEST_ASSERT(commented.getName() == "Macintosh");

    // A link holds nothing of its own, so none of these is a table.
    for (const auto &content : { "-alias macintosh\n41=Z\n", "-name Mac\n-alias macintosh\n",
                                 "-alias macintosh\n-description A link\n", "-alias macintosh\n-alias roman\n" }) {
        const hex::EncodingFile impure(hex::EncodingFile::Type::Thingy, std::string(content), resolve);
        TEST_ASSERT(!impure.valid());
    }

    TEST_SUCCESS();
};

TEST_SEQUENCE("EncodingFileMetadata") {
    // The name keeps the case it is written with.
    const hex::EncodingFile table(hex::EncodingFile::Type::Thingy, std::string(
        "-name Windows-1252\n"
        "-description Latin 1, with the C1 range filled in\n"
        "41=A\n"));

    TEST_ASSERT(table.getName() == "Windows-1252");
    TEST_ASSERT(table.getDescription() == "Latin 1, with the C1 range filled in");
    TEST_ASSERT(table.decodeAll(std::vector<u8>{ 0x41 }) == "A");

    // A keyword a line only starts with is not the directive.
    const hex::EncodingFile notADirective(hex::EncodingFile::Type::Thingy, std::string("-names x\n41=A\n"));
    TEST_ASSERT(notADirective.getName() == "Unknown");

    // A directive below the first entry is not a table.
    const hex::EncodingFile late(hex::EncodingFile::Type::Thingy, std::string(
        "41=A\n"
        "-name Too Late\n"));
    TEST_ASSERT(!late.valid());

    TEST_SUCCESS();
};

TEST_SEQUENCE("EncodingFileAmbiguity") {
    // Two byte sequences with one decoded value cannot be encoded back.
    const hex::EncodingFile duplicateTarget(hex::EncodingFile::Type::Thingy, std::string(
        "80=\xCE\xB1\n"
        "81=\xCE\xB1\n"));
    TEST_ASSERT(duplicateTarget.valid());
    TEST_ASSERT(!duplicateTarget.canEncode());
    TEST_ASSERT(!duplicateTarget.encodeAll("\xCE\xB1").has_value());

    // This table is not prefix-free, so it is not uniquely decodable.
    const hex::EncodingFile prefixed(hex::EncodingFile::Type::Thingy, std::string(
        "80=\xCE\xB1\xCE\xB2\n"
        "81=\xCE\xB1\n"));
    TEST_ASSERT(prefixed.valid());
    TEST_ASSERT(!prefixed.canEncode());

    TEST_SUCCESS();
};

TEST_SEQUENCE("SingleCharacterAndControlCodes") {
    TEST_ASSERT(hex::isSingleCharacter("A"));
    TEST_ASSERT(hex::isSingleCharacter("\xC3\xA9"));
    TEST_ASSERT(hex::isSingleCharacter("\xF0\x9F\x98\x80"));
    TEST_ASSERT(!hex::isSingleCharacter(""));
    TEST_ASSERT(!hex::isSingleCharacter("ab"));
    TEST_ASSERT(!hex::isSingleCharacter("\xC3"));

    // A name like "NUL" is not a character that can be drawn.
    TEST_ASSERT(!hex::isSingleCharacter("NUL"));

    TEST_ASSERT(hex::isControlCode(0x00));
    TEST_ASSERT(hex::isControlCode(0x1F));
    TEST_ASSERT(hex::isControlCode(0x7F));
    TEST_ASSERT(!hex::isControlCode(0x20));
    TEST_ASSERT(!hex::isControlCode(0x41));
    TEST_ASSERT(!hex::isControlCode(0x80));

    // A code point above ASCII is not one, whatever its low byte says.
    TEST_ASSERT(!hex::isControlCode(U'\u2022'));
    TEST_ASSERT(!hex::isControlCode(char32_t(0x0100)));

    // C1 has no picture character of its own, so it is not one either.
    TEST_ASSERT(!hex::isControlCode(char32_t(0x0085)));

    TEST_SUCCESS();
};

TEST_SEQUENCE("EncodingFileNames") {
    TEST_ASSERT(hex::encodingFileName("ascii") == "ascii");
    TEST_ASSERT(hex::encodingFileName("Windows-1252") == "windows_1252");
    TEST_ASSERT(hex::encodingFileName("ANSI_X3.4-1968") == "ansi_x3_4_1968");
    TEST_ASSERT(hex::encodingFileName("Mac OS Roman") == "mac_os_roman");
    TEST_ASSERT(hex::encodingFileName("urn:x-enc") == "urn_x_enc");

    // A directory part becomes part of the name, so a lookup stays in the encodings directory.
    TEST_ASSERT(hex::encodingFileName("../../etc/passwd") == "______etc_passwd");

    TEST_SUCCESS();
};

TEST_SEQUENCE("EncodingLookupRejectsPathTraversal") {
    // A script can name an encoding without the sandbox prompt, so only this directory is in reach.
    TEST_ASSERT(hex::getEncodingByName("../../../etc/passwd") == nullptr);
    TEST_ASSERT(hex::getEncodingByName("/etc/passwd") == nullptr);
    TEST_ASSERT(hex::getEncodingByName("") == nullptr);
    TEST_ASSERT(hex::getEncodingByName("no_such_encoding_exists") == nullptr);

    TEST_SUCCESS();
};

TEST_SEQUENCE("CodepageFromEncoding") {
    const auto &ascii = hex::Codepage::ascii();
    TEST_ASSERT(ascii.getName().empty());
    TEST_ASSERT(ascii['A'] == hex::Codepoint(U'A'));
    TEST_ASSERT(ascii[' '] == hex::Codepoint(U' '));
    TEST_ASSERT(ascii[0x00] == hex::Codepoint(U'\0'));
    TEST_ASSERT(ascii[0x7F] == hex::Codepoint(0x7F));
    TEST_ASSERT(ascii[0x80] == hex::Codepoint::Invalid);
    const hex::EncodingFile singleByte(hex::EncodingFile::Type::Thingy, std::string(
        "80=\xCE\xB1\n"
        "81=\xCE\xB2\n"));
    const auto codepage = hex::Codepage::fromEncoding(singleByte);
    TEST_ASSERT(codepage.has_value());
    TEST_ASSERT((*codepage)[0x80] == hex::Codepoint(U'\u03B1'));
    TEST_ASSERT((*codepage)[0x81] == hex::Codepoint(U'\u03B2'));
    TEST_ASSERT((*codepage)[0x82] == hex::Codepoint::Invalid);

    // A table with a multi byte sequence cannot give every byte its own cell.
    const hex::EncodingFile multiByte(hex::EncodingFile::Type::Thingy, std::string(
        "8140=\xE3\x81\x82\n"));
    TEST_ASSERT(!hex::Codepage::fromEncoding(multiByte).has_value());

    // A byte that draws as more than one code point cannot either.
    const hex::EncodingFile multiCharacter(hex::EncodingFile::Type::Thingy, std::string(
        "80=ab\n"));
    TEST_ASSERT(!hex::Codepage::fromEncoding(multiCharacter).has_value());

    // A control code's name is text, not a character, so this table is not a codepage.
    const hex::EncodingFile namedControlCode(hex::EncodingFile::Type::Thingy, std::string(
        "00=NUL\n"
        "80=\xCE\xB1\n"));
    TEST_ASSERT(!hex::Codepage::fromEncoding(namedControlCode).has_value());

    // A table names a control code with its escape instead.
    const hex::EncodingFile escapedControlCode(hex::EncodingFile::Type::Thingy, std::string(
        "00=\\u0000\n"
        "07=\\u2022\n"
        "25=\\u000A\n"
        "80=\xCE\xB1\n"));
    const auto withControlCodes = hex::Codepage::fromEncoding(escapedControlCode);
    TEST_ASSERT(withControlCodes.has_value());
    TEST_ASSERT((*withControlCodes)[0x00] == hex::Codepoint(U'\0'));
    TEST_ASSERT((*withControlCodes)[0x80] == hex::Codepoint(U'\u03B1'));

    // The character decides what the byte stands for, not the byte itself.
    TEST_ASSERT((*withControlCodes)[0x25] == hex::Codepoint(U'\u000A'));
    TEST_ASSERT((*withControlCodes)[0x07] == hex::Codepoint(U'\u2022'));
    TEST_ASSERT((*withControlCodes)[0x0A] == hex::Codepoint::Invalid);

    TEST_SUCCESS();
};
