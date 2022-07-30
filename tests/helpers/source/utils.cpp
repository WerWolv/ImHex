#include <hex/test/tests.hpp>

#include <hex/helpers/utils.hpp>

using namespace std::literals::string_literals;

TEST_SEQUENCE("SplitStringAtChar") {
    const std::string TestString                   = "Hello|World|ABCD|Test|";
    const std::vector<std::string> TestSplitVector = { "Hello", "World", "ABCD", "Test", "" };

    TEST_ASSERT(hex::splitString(TestString, "|") == TestSplitVector);

    TEST_SUCCESS();
};

TEST_SEQUENCE("SplitStringAtString") {
    const std::string TestString                   = "Hello|DELIM|World|DELIM|ABCD|DELIM|Test|DELIM|";
    const std::vector<std::string> TestSplitVector = { "Hello", "World", "ABCD", "Test", "" };

    TEST_ASSERT(hex::splitString(TestString, "|DELIM|") == TestSplitVector);

    TEST_SUCCESS();
};

TEST_SEQUENCE("ExtractBits") {
    TEST_ASSERT(hex::extract(11, 4, 0xAABBU) == 0xAB);
    TEST_ASSERT(hex::extract(15, 0, 0xAABBU) == 0xAABB);
    TEST_ASSERT(hex::extract(35, 20, 0x8899AABBCCDDEEFFU) == 0xBCCD);
    TEST_ASSERT(hex::extract(20, 35, 0x8899AABBCCDDEEFFU) == 0xBCCD);

    TEST_SUCCESS();
};

TEST_SEQUENCE("DecodeLEB128") {
    TEST_ASSERT(hex::decodeUleb128({}) == 0);
    TEST_ASSERT(hex::decodeUleb128({ 1 }) == 0x01);
    TEST_ASSERT(hex::decodeUleb128({ 0x7F }) == 0x7F);
    TEST_ASSERT(hex::decodeUleb128({ 0xFF }) == 0x7F);
    TEST_ASSERT(hex::decodeUleb128({ 0xFF, 0x7F }) == 0x3FFF);
    TEST_ASSERT(hex::decodeUleb128({
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x7F,
    }) == ((static_cast<u128>(0xFFFF'FFFF'FFFF) << 64) | 0xFFFF'FFFF'FFFF'FFFF));
    TEST_ASSERT(hex::decodeUleb128({ 0xAA, 0xBB, 0xCC, 0x00, 0xFF }) == 0x131DAA);

    TEST_ASSERT(hex::decodeSleb128({}) == 0);
    TEST_ASSERT(hex::decodeSleb128({ 1 }) == 0x01);
    TEST_ASSERT(hex::decodeSleb128({ 0x3F }) == 0x3F);
    TEST_ASSERT(hex::decodeSleb128({ 0x7F }) == -1);
    TEST_ASSERT(hex::decodeSleb128({ 0xFF }) == -1);
    TEST_ASSERT(hex::decodeSleb128({ 0xFF, 0x7F }) == -1);
    TEST_ASSERT(hex::decodeSleb128({
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x7F,
    }) == -1);
    TEST_ASSERT(hex::decodeSleb128({ 0xAA, 0xBB, 0xCC, 0x00, 0xFF }) == 0x131DAA);
    TEST_ASSERT(hex::decodeSleb128({ 0xAA, 0xBB, 0x4C }) == -0xCE256);

    TEST_SUCCESS();
};

TEST_SEQUENCE("EncodeLEB128") {
    TEST_ASSERT(hex::encodeUleb128(0) == (std::vector<u8>{ 0 }));
    TEST_ASSERT(hex::encodeUleb128(0x7F) == (std::vector<u8>{ 0x7F }));
    TEST_ASSERT(hex::encodeUleb128(0xFF) == (std::vector<u8>{ 0xFF, 0x01 }));
    TEST_ASSERT(hex::encodeUleb128(0xF0F0) == (std::vector<u8>{ 0xF0, 0xE1, 0x03 }));

    TEST_ASSERT(hex::encodeSleb128(0) == (std::vector<u8>{ 0 }));
    TEST_ASSERT(hex::encodeSleb128(0x7F) == (std::vector<u8>{ 0xFF, 0x00 }));
    TEST_ASSERT(hex::encodeSleb128(0xFF) == (std::vector<u8>{ 0xFF, 0x01 }));
    TEST_ASSERT(hex::encodeSleb128(0xF0F0) == (std::vector<u8>{ 0xF0, 0xE1, 0x03 }));
    TEST_ASSERT(hex::encodeSleb128(-1) == (std::vector<u8>{ 0x7F }));
    TEST_ASSERT(hex::encodeSleb128(-128) == (std::vector<u8>{ 0x80, 0x7F }));

    TEST_SUCCESS();
};
