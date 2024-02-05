#include <hex/test/tests.hpp>

TEST_SEQUENCE("Test1") {
    TEST_ASSERT(1 == 1, "1 should be equal to 1");
    TEST_SUCCESS();
};