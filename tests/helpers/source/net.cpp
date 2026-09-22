#include <hex/test/tests.hpp>

#include <hex/api/http/store_api.hpp>
#include <hex/api/http/tips_api.hpp>
#include <hex/helpers/http_requests.hpp>
#include <wolv/io/file.hpp>

#include <array>
#include <iterator>

TEST_SEQUENCE("StoreAPI") {
    std::array<std::future<hex::StoreApi::Request>, 8> requests;
    for (auto &request : requests) {
        request = std::async(std::launch::async, [] {
            return hex::StoreApi::get();
        });
    }

    const auto cachedRequest = requests.front().get();
    const auto &result = cachedRequest.get();

    if (!result.isSuccess())
        TEST_FAIL();

    if (result.getData().categories.empty())
        TEST_FAIL();

    for (auto request = std::next(requests.begin()); request != requests.end(); ++request) {
        const auto &cachedResult = request->get().get();
        if (!cachedResult.isSuccess() || cachedResult.getData().categories.size() != result.getData().categories.size())
            TEST_FAIL();
    }

    TEST_SUCCESS();
};

TEST_SEQUENCE("TipsAPI") {
    const auto request = hex::TipsApi::get();
    const auto cachedRequest = hex::TipsApi::get();
    const auto &result = request.get();

    if (!result.isSuccess())
        TEST_FAIL();

    if (result.getData().empty())
        TEST_FAIL();

    if (result.getData() != cachedRequest.get().getData())
        TEST_FAIL();

    TEST_SUCCESS();
};

TEST_SEQUENCE("ContentAPI") {
    hex::HttpRequest request("GET", "https://api.werwolv.net/content/imhex/patterns/elf.hexpat");

    const auto FilePath = std::fs::current_path() / "elf.hexpat";

    auto result = request.downloadFile(FilePath).get();

    TEST_ASSERT(result.getStatusCode() == hex::HttpRequest::HttpStatus(200));

    wolv::io::File file(FilePath, wolv::io::File::Mode::Read);
    if (!file.isValid())
        TEST_FAIL();

    if (file.getSize() == 0)
        TEST_FAIL();

    TEST_SUCCESS();
};
