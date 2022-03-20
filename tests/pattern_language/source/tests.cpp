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
#include "test_patterns/test_pattern_rvalues.hpp"
#include "test_patterns/test_pattern_namespaces.hpp"
#include "test_patterns/test_pattern_extra_semicolon.hpp"
#include "test_patterns/test_pattern_pointers.hpp"
#include "test_patterns/test_pattern_arrays.hpp"
#include "test_patterns/test_pattern_nested_structs.hpp"

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
    TEST(Math),
    TEST(RValues),
    TEST(Namespaces),
    TEST(ExtraSemicolon),
    TEST(Pointers),
    TEST(Arrays),
    TEST(NestedStructs),
};