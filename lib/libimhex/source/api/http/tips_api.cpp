#include <hex/api/http/tips_api.hpp>

#include <hex/api/http/api_urls.hpp>
#include <hex/helpers/http_requests.hpp>

#include <condition_variable>
#include <mutex>
#include <optional>

namespace hex {

    namespace {

        using namespace std::literals::string_literals;
        using namespace std::literals::chrono_literals;

        struct TipsState {
            std::mutex mutex;
            std::condition_variable requestFinished;
            std::optional<TipsApi::Result> cachedResult;
            bool requestInProgress = false;
        };

        TipsState& getTipsState() {
            static TipsState state;
            return state;
        }

        TipsApi::Request makeReadyRequest(const TipsApi::Result &result) {
            std::promise<TipsApi::Result> promise;
            promise.set_value(result);
            return promise.get_future().share();
        }

        TipsApi::Request waitForRequest(TipsState &state) {
            return std::async(std::launch::async, [&state] {
                std::unique_lock lock(state.mutex);
                state.requestFinished.wait(lock, [&state] { return !state.requestInProgress; });
                return *state.cachedResult;
            }).share();
        }

        TipsApi::Request startRequest(TipsState &state) {
            state.requestInProgress = true;
            return std::async(std::launch::async, [&state] {
                auto result = [] {
                    try {
                        HttpRequest request("GET", ImHexApiURL + "/tip"s);
                        request.setTimeout(30'000);
                        const auto response = request.execute().get();
                        if (!response.isSuccess())
                            return TipsApi::Result(TipsApi::Result::Status::NetworkError, { }, response.getStatusCode().toString());

                        return TipsApi::Result(TipsApi::Result::Status::Success, response.getData());
                    } catch (const std::exception &error) {
                        return TipsApi::Result(TipsApi::Result::Status::NetworkError, { }, error.what());
                    }
                }();

                {
                    std::scoped_lock lock(state.mutex);
                    state.cachedResult = result;
                    state.requestInProgress = false;
                }
                state.requestFinished.notify_all();
                return result;
            }).share();
        }

    }

    TipsApi::Request TipsApi::get() {
        auto &state = getTipsState();
        std::scoped_lock lock(state.mutex);
        if (state.requestInProgress)
            return waitForRequest(state);
        if (state.cachedResult.has_value())
            return makeReadyRequest(*state.cachedResult);

        return startRequest(state);
    }

    TipsApi::Request TipsApi::refresh() {
        auto &state = getTipsState();
        std::scoped_lock lock(state.mutex);
        if (state.requestInProgress)
            return waitForRequest(state);

        return startRequest(state);
    }

}
