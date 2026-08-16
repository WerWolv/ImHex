#include <hex/test/tests.hpp>

#include <hex/helpers/encoding_file.hpp>

#include <vector>

TEST_SEQUENCE("EncodingLineStartAddressCache") {
    std::vector<u64> lineStartAddresses = { 0, 2, 1 };

    hex::impl::appendEncodingLineStartAddress(lineStartAddresses, 0, 3);
    TEST_ASSERT(lineStartAddresses == std::vector<u64>({ 0, 2, 1 }));

    hex::impl::appendEncodingLineStartAddress(lineStartAddresses, 2, 3);
    TEST_ASSERT(lineStartAddresses == std::vector<u64>({ 0, 2, 1, 3 }));

    TEST_SUCCESS();
};
