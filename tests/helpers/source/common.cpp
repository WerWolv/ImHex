#include <hex/test/tests.hpp>
#include <hex/test/test_provider.hpp>

#include <hex/helpers/crypto.hpp>

#include <algorithm>
#include <vector>

TEST_SEQUENCE("TestSucceeding") {
    TEST_SUCCESS();
};

TEST_SEQUENCE("TestFailing", FAILING) {
    TEST_FAIL();
};

TEST_SEQUENCE("TestProvider_read") {
    std::vector<u8> data { 0xde, 0xad, 0xbe, 0xef, 0x42, 0x2a, 0x00, 0xff };
    hex::test::TestProvider provider(&data);
    hex::prv::Provider *provider2 = &provider;

    u8 buff[1024];

    std::fill(std::begin(buff), std::end(buff), 22);
    provider2->read(0, buff + 1, 4);
    TEST_ASSERT(buff[0] == 22);    // should be unchanged
    TEST_ASSERT(buff[1] == 0xde);
    TEST_ASSERT(buff[2] == 0xad);
    TEST_ASSERT(buff[3] == 0xbe);
    TEST_ASSERT(buff[4] == 0xef);
    TEST_ASSERT(buff[5] == 22);    // should be unchanged

    std::fill(std::begin(buff), std::end(buff), 22);
    provider2->read(6, buff, 2);
    TEST_ASSERT(buff[0] == 0x00);
    TEST_ASSERT(buff[1] == 0xff);
    TEST_ASSERT(buff[2] == 22);    // should be unchanged

    std::fill(std::begin(buff), std::end(buff), 22);
    provider2->read(7, buff, 2);
    TEST_ASSERT(std::count(std::begin(buff), std::end(buff), 22) == std::size(buff));    // buff should be unchanged

    TEST_SUCCESS();
};

TEST_SEQUENCE("TestProvider_write") {
    std::vector<u8> buff(8);
    hex::test::TestProvider provider(&buff);
    hex::prv::Provider *provider2 = &provider;

    u8 data[1024] = { 0xde, 0xad, 0xbe, 0xef, 0x42, 0x2a, 0x00, 0xff };

    std::fill(std::begin(buff), std::end(buff), 22);
    provider2->writeRaw(1, data, 4);
    TEST_ASSERT(buff[0] == 22);    // should be unchanged
    TEST_ASSERT(buff[1] == 0xde);
    TEST_ASSERT(buff[2] == 0xad);
    TEST_ASSERT(buff[3] == 0xbe);
    TEST_ASSERT(buff[4] == 0xef);
    TEST_ASSERT(buff[5] == 22);    // should be unchanged

    std::fill(std::begin(buff), std::end(buff), 22);
    provider2->writeRaw(0, data + 6, 2);
    TEST_ASSERT(buff[0] == 0x00);
    TEST_ASSERT(buff[1] == 0xff);
    TEST_ASSERT(buff[2] == 22);    // should be unchanged

    std::fill(std::begin(buff), std::end(buff), 22);
    provider2->writeRaw(6, data, 2);
    TEST_ASSERT(buff[5] == 22);    // should be unchanged
    TEST_ASSERT(buff[6] == 0xde);
    TEST_ASSERT(buff[7] == 0xad);

    std::fill(std::begin(buff), std::end(buff), 22);
    provider2->writeRaw(7, data, 2);
    TEST_ASSERT(std::count(std::begin(buff), std::end(buff), 22) == std::size(buff));    // buff should be unchanged

    TEST_SUCCESS();
};
