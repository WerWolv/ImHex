#include <algorithm>
#include <hex/api/http/store_api.hpp>

#include <hex/api/http/api_urls.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/logger.hpp>

#include <nlohmann/json.hpp>

#include <memory>
#include <mutex>
#include <ranges>

namespace hex {

    namespace {

        using namespace std::literals::string_literals;
        using namespace std::literals::chrono_literals;

        struct StoreState {
            std::mutex mutex;
            StoreApi::Request cachedRequest;
        };

        StoreState& getStoreState() {
            static StoreState state;
            return state;
        }

        StoreApi::Result parseStore(const HttpRequest::Result<std::string> &response) {
            if (!response.isSuccess())
                return StoreApi::Result(StoreApi::Result::Status::NetworkError, { }, response.getStatusCode().toString());

            try {
                StoreApi::Store store;
                const auto json = nlohmann::json::parse(response.getData());

                for (const auto &[categoryName, category] : json.items()) {
                    if (!category.is_array())
                        continue;

                    auto &entries = store.categories[categoryName];
                    for (const auto &entry : category) {
                        try {
                            StoreApi::Pragmas pragmas;
                            for (const auto &[name, values] : entry["pragmas"].items()) {
                                for (const auto &value : values)
                                    pragmas.emplace(name, value.get<std::string>());
                            }

                            entries.emplace_back(
                                entry["name"],
                                entry["desc"],
                                entry["authors"],
                                entry["file"],
                                HttpRequest::curlify(entry["url"]),
                                entry["hash"],
                                entry["folder"],
                                std::move(pragmas)
                            );
                        } catch (const nlohmann::json::exception &error) {
                            log::error("Failed to parse store entry: {}", error.what());
                        }
                    }

                    std::ranges::sort(entries, [](const auto &lhs, const auto &rhs) {
                        return lhs.name < rhs.name;
                    });
                }

                return StoreApi::Result(StoreApi::Result::Status::Success, std::move(store));
            } catch (const nlohmann::json::exception &error) {
                return StoreApi::Result(StoreApi::Result::Status::InvalidResponse, { }, error.what());
            }
        }

        StoreApi::Request startRequest() {
            auto httpRequest = std::make_shared<HttpRequest>("GET", ImHexApiURL + "/store"s);
            httpRequest->setTimeout(30'000);
            auto request = httpRequest->execute();

            return std::async(std::launch::async, [httpRequest = std::move(httpRequest), request = std::move(request)]() mutable {
                std::ignore = httpRequest;

                return parseStore(request.get());
            }).share();
        }

    }

    StoreApi::Request StoreApi::get() {
        auto &state = getStoreState();
        std::scoped_lock lock(state.mutex);
        if (!state.cachedRequest.valid())
            state.cachedRequest = startRequest();

        return state.cachedRequest;
    }

    StoreApi::Request StoreApi::refresh() {
        auto &state = getStoreState();
        std::scoped_lock lock(state.mutex);
        if (state.cachedRequest.valid() && state.cachedRequest.wait_for(0s) != std::future_status::ready)
            return state.cachedRequest;

        state.cachedRequest = startRequest();
        return state.cachedRequest;
    }

    StoreApi::DownloadRequest StoreApi::download(const paths::impl::DefaultPath *pathType, const std::string &fileName, const std::string &url) {
        std::fs::path downloadPath;
        for (const auto &folderPath : pathType == nullptr ? std::vector<std::fs::path> { } : pathType->write()) {
            if (!fs::isPathWritable(folderPath))
                continue;

            const auto normalizedFolder = std::fs::absolute(folderPath).lexically_normal();
            const auto fullPath = std::fs::absolute(folderPath / std::fs::path(fileName)).lexically_normal();
            const auto [folderIter, _] = std::ranges::mismatch(normalizedFolder, fullPath);

            if (folderIter != normalizedFolder.end())
                continue;

            downloadPath = fullPath;
            break;
        }

        if (downloadPath.empty()) {
            std::promise<DownloadResult> result;
            result.set_value(DownloadResult(DownloadResult::Status::NoWritablePath));
            return result.get_future();
        }

        auto httpRequest = std::make_shared<HttpRequest>("GET", url);
        httpRequest->setTimeout(30'000);
        auto request = httpRequest->downloadFile(downloadPath);

        return std::async(std::launch::async, [httpRequest = std::move(httpRequest), request = std::move(request), downloadPath]() mutable {
            std::ignore = httpRequest;

            const auto response = request.get();
            if (!response.isSuccess()) {
                std::error_code error;
                std::filesystem::remove(downloadPath, error);
                return DownloadResult(DownloadResult::Status::NetworkError, downloadPath, response.getStatusCode().toString());
            }

            return DownloadResult(DownloadResult::Status::Success, downloadPath);
        });
    }

}
