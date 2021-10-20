#include <hex/helpers/crypto.hpp>
#include "test_provider.hpp"
#include "tests.hpp"

TEST_SEQUENCE("TestSucceeding") {
    TEST_SUCCESS();
};

TEST_SEQUENCE("TestFailing", FAILING) {
    TEST_FAIL();
};