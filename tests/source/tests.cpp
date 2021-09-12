#include <array>

#include "test_patterns/test_pattern_placement.hpp"
#include "test_patterns/test_pattern_structs.hpp"
#include "test_patterns/test_pattern_unions.hpp"
#include "test_patterns/test_pattern_enums.hpp"
#include "test_patterns/test_pattern_literals.hpp"
#include "test_patterns/test_pattern_padding.hpp"
#include "test_patterns/test_pattern_succeeding_assert.hpp"
#include "test_patterns/test_pattern_failing_assert.hpp"
#include "test_patterns/test_pattern_bitfields.hpp"
#include "test_patterns/test_pattern_math.hpp"

std::array Tests = {
        TEST(Placement),
        TEST(Structs),
        TEST(Unions),
        TEST(Enums),
        TEST(Literals),
        TEST(Padding),
        TEST(SucceedingAssert),
        TEST(FailingAssert),
        TEST(Bitfields),
        TEST(Math)
};