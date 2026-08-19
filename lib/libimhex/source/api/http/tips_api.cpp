#include <hex/api/http/tips_api.hpp>

#include <hex/api/http/api_urls.hpp>
#include <hex/helpers/http_requests.hpp>

#include <memory>
#include <mutex>

namespace hex {

    namespace {

        using namespace std::literals::string_literals;
        using namespace std::literals::chrono_literals;

        struct TipsState {
            std::mutex mutex;
            TipsApi::Request cachedRequest;
        };

        TipsState& getTipsState() {
            static TipsState state;
            return state;
        }

        TipsApi::Request startRequest() {
            auto httpRequest = std::make_shared<HttpRequest>("GET", ImHexApiURL + "/tip"s);
            httpRequest->setTimeout(30'000);
            auto request = httpRequest->execute();

            return std::async(std::launch::async, [httpRequest = std::move(httpRequest), request = std::move(request)]() mutable {
                static_cast<void>(httpRequest);
                auto response = request.get();
                if (!response.isSuccess())
                    return TipsApi::Result(TipsApi::Result::Status::NetworkError, { }, response.getStatusCode().toString());

                return TipsApi::Result(TipsApi::Result::Status::Success, response.getData());
            }).share();
        }

    }

    TipsApi::Request TipsApi::get() {
        auto &state = getTipsState();
        std::scoped_lock lock(state.mutex);
        if (!state.cachedRequest.valid())
            state.cachedRequest = startRequest();

        return state.cachedRequest;
    }

    TipsApi::Request TipsApi::refresh() {
        auto &state = getTipsState();
        std::scoped_lock lock(state.mutex);
        if (state.cachedRequest.valid() && state.cachedRequest.wait_for(0s) != std::future_status::ready)
            return state.cachedRequest;

        state.cachedRequest = startRequest();
        return state.cachedRequest;
    }

}
