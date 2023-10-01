#pragma once

#include <hex.hpp>

#include <future>
#include <map>
#include <string>
#include <vector>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/fmt.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/core.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

#include <mbedtls/ssl.h>

#if defined(OS_EMSCRIPTEN)
#include<emscripten/fetch.h>
#define curl_off_t long
#else
#include <curl/curl.h>
#endif

namespace hex {

    class HttpRequest {
    public:

        class ResultBase {
        public:
            ResultBase() = default;
            explicit ResultBase(u32 statusCode) : m_statusCode(statusCode), m_valid(true) { }

            [[nodiscard]] u32 getStatusCode() const {
                return this->m_statusCode;
            }

            [[nodiscard]] bool isSuccess() const {
                return this->getStatusCode() == 200;
            }

            [[nodiscard]] bool isValid() const {
                return this->m_valid;
            }

        private:
            u32 m_statusCode = 0;
            bool m_valid = false;
        };

        template<typename T>
        class Result : public ResultBase {
        public:
            Result() = default;
            Result(u32 statusCode, T data) : ResultBase(statusCode), m_data(std::move(data)) { }

            [[nodiscard]]
            const T& getData() const {
                return this->m_data;
            }

        private:
            T m_data;
        };

        HttpRequest(std::string method, std::string url);
        ~HttpRequest();

        HttpRequest(const HttpRequest&) = delete;
        HttpRequest& operator=(const HttpRequest&) = delete;

        HttpRequest(HttpRequest &&other) noexcept;

        HttpRequest& operator=(HttpRequest &&other) noexcept;

        static void setProxy(std::string proxy);

        void setMethod(std::string method) {
            this->m_method = std::move(method);
        }

        void setUrl(std::string url) {
            this->m_url = std::move(url);
        }

        void addHeader(std::string key, std::string value) {
            this->m_headers[std::move(key)] = std::move(value);
        }

        void setBody(std::string body) {
            this->m_body = std::move(body);
        }

        void setTimeout(u32 timeout) {
            this->m_timeout = timeout;
        }

        float getProgress() const {
            return this->m_progress;
        }

        void cancel() {
            this->m_canceled = true;
        }

        template<typename T = std::string>
        std::future<Result<T>> downloadFile(const std::fs::path &path);

        std::future<Result<std::vector<u8>>> downloadFile();

        template<typename T = std::string>
        std::future<Result<T>> uploadFile(const std::fs::path &path, const std::string &mimeName = "filename");

        template<typename T = std::string>
        std::future<Result<T>> uploadFile(std::vector<u8> data, const std::string &mimeName = "filename", const std::fs::path &fileName = "data.bin");

        template<typename T = std::string>
        std::future<Result<T>> execute();

        std::string urlEncode(const std::string &input);

        std::string urlDecode(const std::string &input);

    protected:
        void setDefaultConfig();

        template<typename T>
        Result<T> executeImpl(std::vector<u8> &data);

        static size_t writeToVector(void *contents, size_t size, size_t nmemb, void *userdata);
        static size_t writeToFile(void *contents, size_t size, size_t nmemb, void *userdata);
        static int progressCallback(void *contents, curl_off_t dlTotal, curl_off_t dlNow, curl_off_t ulTotal, curl_off_t ulNow);

    private:
        static void checkProxyErrors();

    private:
        #if defined(OS_EMSCRIPTEN)
        emscripten_fetch_attr_t m_attr;
        #else
        CURL *m_curl;
        #endif

        std::mutex m_transmissionMutex;

        std::string m_method;
        std::string m_url;
        std::string m_body;
        std::promise<std::vector<u8>> m_promise;
        std::map<std::string, std::string> m_headers;
        u32 m_timeout = 1000;

        std::atomic<float> m_progress = 0.0F;
        std::atomic<bool> m_canceled = false;
    };
}


#if defined(OS_EMSCRIPTEN)
#include <hex/helpers/http_requests_emscripten.hpp>
#else
#include <hex/helpers/http_requests_native.hpp>
#endif
