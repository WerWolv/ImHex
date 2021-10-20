#include <hex/helpers/utils.hpp>
#include "test_provider.hpp"
#include "tests.hpp"

TEST_SEQUENCE("32BitIntegerEndianSwap") {
    TEST_ASSERT(hex::changeEndianess<u32>(0xAABBCCDD, std::endian::big) == 0xDDCCBBAA);
};

TEST_SEQUENCE("64BitFloatEndianSwap", FAILING) {
    double floatValue = 1234.5;
    u64 integerValue = reinterpret_cast<u64&>(floatValue);

    double swappedFloatValue = hex::changeEndianess(floatValue, std::endian::big);
    u64 swappedIntegerValue = hex::changeEndianess(integerValue, std::endian::big);

    TEST_ASSERT(std::memcmp(&floatValue, &integerValue, 8) && std::memcmp(&swappedFloatValue, &swappedIntegerValue, 8));
};