#include <hex/test/tests.hpp>

#include <hex/helpers/utils.hpp>

#include <vector>

using namespace std::literals::string_literals;

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
    TEST_ASSERT(hex::decodeByteString(hex::encodeByteString(backslashThenN)) == backslashThenN);

    const std::vector<u8> bytes = { 0x00, 0x41, 0x0A, 0x5C, 0x5C, 0x6E, 0xFF };
    TEST_ASSERT(hex::encodeByteString(bytes) == "\\x00A\\n\\\\\\\\n\\xFF");
    TEST_ASSERT(hex::decodeByteString(hex::encodeByteString(bytes)) == bytes);

    TEST_SUCCESS();
};
