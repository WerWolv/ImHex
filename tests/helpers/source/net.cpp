#include <hex/test/tests.hpp>

#include <hex/helpers/net.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/fs.hpp>

using namespace std::literals::string_literals;

TEST_SEQUENCE("StoreAPI") {
    hex::Net net;

    auto result = net.getString(ImHexApiURL + "/store"s).get();

    if (result.code != 200)
        TEST_FAIL();

    if (result.body.empty())
        TEST_FAIL();

    TEST_SUCCESS();
};

TEST_SEQUENCE("TipsAPI") {
    hex::Net net;

    auto result = net.getString(ImHexApiURL + "/tip"s).get();

    if (result.code != 200)
        TEST_FAIL();

    if (result.body.empty())
        TEST_FAIL();

    TEST_SUCCESS();
};

TEST_SEQUENCE("ContentAPI") {
    hex::Net net;

    const auto FilePath = std::fs::current_path() / "elf.hexpat";

    auto result = net.downloadFile("https://api.werwolv.net/content/imhex/patterns/elf.hexpat", FilePath).get();

    TEST_ASSERT(result.code == 200);

    hex::fs::File file(FilePath, hex::fs::File::Mode::Read);
    if (!file.isValid())
        TEST_FAIL();

    if (file.getSize() == 0)
        TEST_FAIL();

    TEST_SUCCESS();
};