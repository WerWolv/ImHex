#include <hex/helpers/utils.hpp>
#include <hex/test/test_provider.hpp>
#include <hex/test/tests.hpp>

TEST_SEQUENCE("32BitIntegerEndianSwap") {
    TEST_ASSERT(hex::changeEndianness<u32>(0xAABBCCDD, std::endian::big) == 0xDDCCBBAA);

    TEST_SUCCESS();
};

TEST_SEQUENCE("64BitFloatEndianSwap") {
    double floatValue = 1234.5;
    u64 integerValue  = reinterpret_cast<u64 &>(floatValue);

    double swappedFloatValue = hex::changeEndianness(floatValue, std::endian::big);
    u64 swappedIntegerValue  = hex::changeEndianness(integerValue, std::endian::big);

    TEST_ASSERT(std::memcmp(&floatValue, &integerValue, 8) == 0 && std::memcmp(&swappedFloatValue, &swappedIntegerValue, 8) == 0);

    TEST_SUCCESS();
};
