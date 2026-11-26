#include <hex/test/tests.hpp>

#include <hex/helpers/binary_pattern.hpp>

TEST_SEQUENCE("BinaryPatternValueWithSign") {
    const hex::BinaryPattern plus("u8(+5)");
    TEST_ASSERT(plus.isValid());
    TEST_ASSERT(plus.getSize() == 1);
    TEST_ASSERT(plus.matches({ 0x05 }));

    const hex::BinaryPattern plusBigEndian("u16be(+258)");
    TEST_ASSERT(plusBigEndian.isValid());
    TEST_ASSERT(plusBigEndian.matches({ 0x01, 0x02 }));

    const hex::BinaryPattern minus("s16(-2)");
    TEST_ASSERT(minus.isValid());
    TEST_ASSERT(minus.matches({ 0xFE, 0xFF }));

    TEST_SUCCESS();
};
