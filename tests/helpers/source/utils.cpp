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
