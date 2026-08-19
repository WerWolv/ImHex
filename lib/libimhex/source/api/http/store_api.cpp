#include <hex/api/http/store_api.hpp>

#include <hex/api/http/api_urls.hpp>
#include <hex/helpers/http_requests.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <memory>
#include <mutex>

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
                store.pragmas = json.contains("pragmas")
                    ? json["pragmas"].get<StoreApi::Pragmas>()
                    : StoreApi::Pragmas { };

                for (const auto &[categoryName, category] : json.items()) {
                    if (!category.is_array())
                        continue;

                    auto &entries = store.categories[categoryName];
                    for (const auto &entry : category) {
                        if (!entry.contains("name") || !entry.contains("desc") || !entry.contains("authors") ||
                            !entry.contains("file") || !entry.contains("url") || !entry.contains("hash") || !entry.contains("folder"))
                            continue;

                        entries.push_back({
                            entry["name"],
                            entry["desc"],
                            entry["authors"],
                            entry["file"],
                            HttpRequest::curlify(entry["url"]),
                            entry["hash"],
                            entry["folder"]
                        });
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
                static_cast<void>(httpRequest);
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

}
