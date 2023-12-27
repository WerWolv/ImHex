#include <hex/test/tests.hpp>

#include <hex/api_urls.hpp>
#include <hex/helpers/http_requests.hpp>
#include <wolv/io/file.hpp>

using namespace std::literals::string_literals;

TEST_SEQUENCE("StoreAPI") {
    hex::HttpRequest request("GET", ImHexApiURL + "/store"s);

    auto result = request.execute().get();

    if (result.getStatusCode() != 200)
        TEST_FAIL();

    if (result.getData().empty())
        TEST_FAIL();

    TEST_SUCCESS();
};

TEST_SEQUENCE("TipsAPI") {
    hex::HttpRequest request("GET", ImHexApiURL + "/tip"s);

    auto result = request.execute().get();

    if (result.getStatusCode() != 200)
        TEST_FAIL();

    if (result.getData().empty())
        TEST_FAIL();

    TEST_SUCCESS();
};

TEST_SEQUENCE("ContentAPI") {
    hex::HttpRequest request("GET", "https://api.werwolv.net/content/imhex/patterns/elf.hexpat");

    const auto FilePath = std::fs::current_path() / "elf.hexpat";

    auto result = request.downloadFile(FilePath).get();

    TEST_ASSERT(result.getStatusCode() == 200);

    wolv::io::File file(FilePath, wolv::io::File::Mode::Read);
    if (!file.isValid())
        TEST_FAIL();

    if (file.getSize() == 0)
        TEST_FAIL();

    TEST_SUCCESS();
};