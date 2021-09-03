#pragma once

#include <hex.hpp>

#include <cstring>
#include <future>
#include <optional>
#include <string>
#include <filesystem>
#include <atomic>

#include <nlohmann/json_fwd.hpp>

#include <curl/curl.h>

namespace hex {

    template<typename T>
    struct Response {
        s32 code;
        T body;
    };

    template<>
    struct Response<void> {
        s32 code;
    };

    class Net {
    public:
        Net();
        ~Net();

        std::future<Response<std::string>> getString(const std::string &url);
        std::future<Response<nlohmann::json>> getJson(const std::string &url);

        std::future<Response<std::string>> uploadFile(const std::string &url, const std::filesystem::path &filePath);
        std::future<Response<void>> downloadFile(const std::string &url, const std::filesystem::path &filePath);

        [[nodiscard]]
        std::string encode(const std::string &input) {
            auto escapedString = curl_easy_escape(this->m_ctx, input.c_str(), std::strlen(input.c_str()));

            if (escapedString != nullptr) {
                std::string output = escapedString;
                curl_free(escapedString);

                return output;
            }

            return { };
        }

        [[nodiscard]]
        float getProgress() const { return this->m_progress; }

        void cancel() { this->m_shouldCancel = true; }

    private:
        void setCommonSettings(std::string &response, const std::string &url, const std::map<std::string, std::string> &extraHeaders = { }, const std::string &body = { });
        std::optional<s32> execute();

        friend int progressCallback(void *contents, curl_off_t dlTotal, curl_off_t dlNow, curl_off_t ulTotal, curl_off_t ulNow);
    private:
        CURL *m_ctx;
        struct curl_slist *m_headers = nullptr;

        std::mutex m_transmissionActive;
        float m_progress = 0.0F;
        bool m_shouldCancel = false;
    };

}