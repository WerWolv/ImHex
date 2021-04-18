#pragma once

#include <hex.hpp>
#include <future>

#include <curl/curl.h>
#include <nlohmann/json.hpp>


namespace hex {

    template<typename T>
    struct Response {
        u32 code;
        T response;
    };

    class Net {
    public:
        Net();
        ~Net();

        Response<std::string> getString(std::string_view url);
        Response<nlohmann::json> getJson(std::string_view url);

    private:
        CURL *m_ctx;
    };

}