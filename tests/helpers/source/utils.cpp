#include <hex/test/tests.hpp>

#include <hex/helpers/utils.hpp>

#include <vector>

using namespace std::literals::string_literals;

namespace {

    std::vector<u8> bytesOf(std::string_view text) {
        return { text.begin(), text.end() };
    }

}

TEST_SEQUENCE("ExtractBits") {
    TEST_ASSERT(hex::extract(11, 4, 0xAABBU) == 0xAB);
    TEST_ASSERT(hex::extract(15, 0, 0xAABBU) == 0xAABB);
    TEST_ASSERT(hex::extract(35, 20, 0x8899AABBCCDDEEFFU) == 0xBCCD);
    TEST_ASSERT(hex::extract(20, 35, 0x8899AABBCCDDEEFFU) == 0xBCCD);

    TEST_SUCCESS();
};

TEST_SEQUENCE("EncodeByteString") {
    const auto encode = [](u8 byte) { return hex::encodeByteString({ byte }); };

    // A printable ASCII byte stands for itself.
    TEST_ASSERT(encode('A') == "A");
    TEST_ASSERT(encode(' ') == " ");

    // A byte with a named escape gets it.
    TEST_ASSERT(encode('\\') == "\\\\");
    TEST_ASSERT(encode('\a') == "\\a");
    TEST_ASSERT(encode('\b') == "\\b");
    TEST_ASSERT(encode('\f') == "\\f");
    TEST_ASSERT(encode('\n') == "\\n");
    TEST_ASSERT(encode('\r') == "\\r");
    TEST_ASSERT(encode('\t') == "\\t");
    TEST_ASSERT(encode('\v') == "\\v");

    // Everything else becomes \xNN.
    TEST_ASSERT(encode(0x00) == "\\x00");
    TEST_ASSERT(encode(0x1F) == "\\x1F");
    TEST_ASSERT(encode(0x7F) == "\\x7F");
    TEST_ASSERT(encode(0x80) == "\\x80");
    TEST_ASSERT(encode(0xE9) == "\\xE9");
    TEST_ASSERT(encode(0xFF) == "\\xFF");

    TEST_SUCCESS();
};

TEST_SEQUENCE("EncodeByteStringRoundTrip") {
    const std::vector<u8> backslashThenN = { 0x5C, 0x6E };
    TEST_ASSERT(hex::encodeByteString(backslashThenN) == "\\\\n");
    TEST_ASSERT(hex::decodeByteString(hex::encodeByteString(backslashThenN)).value() == backslashThenN);

    const std::vector<u8> bytes = { 0x00, 0x41, 0x0A, 0x5C, 0x5C, 0x6E, 0xFF };
    TEST_ASSERT(hex::encodeByteString(bytes) == "\\x00A\\n\\\\\\\\n\\xFF");
    TEST_ASSERT(hex::decodeByteString(hex::encodeByteString(bytes)).value() == bytes);

    TEST_SUCCESS();
};

TEST_SEQUENCE("DecodeByteStringEscapes") {
    const auto decode = [](const std::string &string) { return hex::decodeByteString(string); };

    TEST_ASSERT(decode("").value() == std::vector<u8>{});
    TEST_ASSERT(decode("abc").value() == bytesOf("abc"));
    TEST_ASSERT(decode("\\n").value() == std::vector<u8>{ 0x0A });
    TEST_ASSERT(decode("\\x41").value() == std::vector<u8>{ 0x41 });
    TEST_ASSERT(decode("\\0").value() == std::vector<u8>{ 0x00 });

    // \u and \U both name a Unicode scalar value and encode it as UTF-8.
    TEST_ASSERT(decode("\\u0041").value() == std::vector<u8>{ 0x41 });
    TEST_ASSERT(decode("\\u00E9").value() == (std::vector<u8>{ 0xC3, 0xA9 }));
    TEST_ASSERT(decode("\\u20AC").value() == (std::vector<u8>{ 0xE2, 0x82, 0xAC }));
    TEST_ASSERT(decode("\\U00000041").value() == std::vector<u8>{ 0x41 });
    TEST_ASSERT(decode("\\U0001F600").value() == (std::vector<u8>{ 0xF0, 0x9F, 0x98, 0x80 }));

    // A surrogate half is not a scalar value, so it has no UTF-8 encoding.
    TEST_ASSERT(!decode("\\uD800").has_value());
    TEST_ASSERT(!decode("\\uDFFF").has_value());
    TEST_ASSERT(!decode("\\U0000D800").has_value());

    // Past the last code point in the Unicode codespace.
    TEST_ASSERT(!decode("\\U00110000").has_value());

    // Malformed escapes.
    TEST_ASSERT(!decode("\\q").has_value());
    TEST_ASSERT(!decode("\\u12").has_value());
    TEST_ASSERT(!decode("\\u12ZZ").has_value());
    TEST_ASSERT(!decode("\\U0001F60").has_value());
    TEST_ASSERT(!decode("\\x4").has_value());

    TEST_SUCCESS();
};
