#include <hex/test/tests.hpp>

#include <hex/helpers/utils.hpp>

using namespace std::literals::string_literals;

TEST_SEQUENCE("ExtractBits") {
    TEST_ASSERT(hex::extract(11, 4, 0xAABBU) == 0xAB);
    TEST_ASSERT(hex::extract(15, 0, 0xAABBU) == 0xAABB);
    TEST_ASSERT(hex::extract(35, 20, 0x8899AABBCCDDEEFFU) == 0xBCCD);
    TEST_ASSERT(hex::extract(20, 35, 0x8899AABBCCDDEEFFU) == 0xBCCD);

    TEST_SUCCESS();
};
