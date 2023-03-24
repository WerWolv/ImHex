#pragma once

#include <hex.hpp>

#include <future>
#include <map>
#include <string>
#include <vector>

#include <curl/curl.h>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/fmt.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/core.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

#include <mbedtls/ssl.h>

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

        HttpRequest(std::string method, std::string url) : m_method(std::move(method)), m_url(std::move(url)) {
            AT_FIRST_TIME {
                curl_global_init(CURL_GLOBAL_ALL);
            };

            AT_FINAL_CLEANUP {
                curl_global_cleanup();
            };

            this->m_curl = curl_easy_init();
        }

        ~HttpRequest() {
            curl_easy_cleanup(this->m_curl);
        }

        HttpRequest(const HttpRequest&) = delete;
        HttpRequest& operator=(const HttpRequest&) = delete;

        HttpRequest(HttpRequest &&other) noexcept {
            this->m_curl = other.m_curl;
            other.m_curl = nullptr;

            this->m_method = std::move(other.m_method);
            this->m_url = std::move(other.m_url);
            this->m_headers = std::move(other.m_headers);
            this->m_body = std::move(other.m_body);

            this->m_caCert = std::move(other.m_caCert);
        }

        HttpRequest& operator=(HttpRequest &&other) noexcept {
            this->m_curl = other.m_curl;
            other.m_curl = nullptr;

            this->m_method = std::move(other.m_method);
            this->m_url = std::move(other.m_url);
            this->m_headers = std::move(other.m_headers);
            this->m_body = std::move(other.m_body);

            this->m_caCert = std::move(other.m_caCert);

            return *this;
        }

        static void setCACert(std::string data) {
            HttpRequest::s_caCertData = std::move(data);
        }

        static void setProxy(std::string proxy) {
            HttpRequest::s_proxyUrl = std::move(proxy);
        }

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
        std::future<Result<T>> downloadFile(const std::fs::path &path) {
            return std::async(std::launch::async, [this, path] {
                std::vector<u8> response;

                wolv::io::File file(path, wolv::io::File::Mode::Create);
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, writeToFile);
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &file);

                return this->executeImpl<T>(response);
            });
        }

        std::future<Result<std::vector<u8>>> downloadFile() {
            return std::async(std::launch::async, [this] {
                std::vector<u8> response;

                curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, writeToVector);
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &response);

                return this->executeImpl<std::vector<u8>>(response);
            });
        }

        template<typename T = std::string>
        std::future<Result<T>> uploadFile(const std::fs::path &path, const std::string &mimeName = "filename") {
            return std::async(std::launch::async, [this, path, mimeName]{
                auto fileName = wolv::util::toUTF8String(path.filename());

                curl_mime *mime     = curl_mime_init(this->m_curl);
                curl_mimepart *part = curl_mime_addpart(mime);

                wolv::io::File file(path, wolv::io::File::Mode::Read);

                curl_mime_data_cb(part, file.getSize(),
                                  [](char *buffer, size_t size, size_t nitems, void *arg) -> size_t {
                                      auto file = static_cast<FILE*>(arg);

                                      return fread(buffer, size, nitems, file);
                                  },
                                  [](void *arg, curl_off_t offset, int origin) -> int {
                                      auto file = static_cast<FILE*>(arg);

                                      if (fseek(file, offset, origin) != 0)
                                          return CURL_SEEKFUNC_CANTSEEK;
                                      else
                                          return CURL_SEEKFUNC_OK;
                                  },
                                  [](void *arg) {
                                      auto file = static_cast<FILE*>(arg);

                                      fclose(file);
                                  },
                                  file.getHandle());
                curl_mime_filename(part, fileName.c_str());
                curl_mime_name(part, mimeName.c_str());

                curl_easy_setopt(this->m_curl, CURLOPT_MIMEPOST, mime);

                std::vector<u8> responseData;
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, writeToVector);
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &responseData);

                return this->executeImpl<T>(responseData);
            });
        }
        template<typename T = std::string>
        std::future<Result<T>> uploadFile(std::vector<u8> data, const std::string &mimeName = "filename") {
            return std::async(std::launch::async, [this, data = std::move(data), mimeName]{
                curl_mime *mime     = curl_mime_init(this->m_curl);
                curl_mimepart *part = curl_mime_addpart(mime);

                curl_mime_data(part, reinterpret_cast<const char *>(data.data()), data.size());
                curl_mime_filename(part, "data.bin");
                curl_mime_name(part, mimeName.c_str());

                curl_easy_setopt(this->m_curl, CURLOPT_MIMEPOST, mime);

                std::vector<u8> responseData;
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, writeToVector);
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &responseData);

                return this->executeImpl<T>(responseData);
            });
        }

        template<typename T = std::string>
        std::future<Result<T>> execute() {
            return std::async(std::launch::async, [this] {

                std::vector<u8> responseData;
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, writeToVector);
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &responseData);

                return this->executeImpl<T>(responseData);
            });
        }

        std::string urlEncode(const std::string &input) {
            auto escapedString = curl_easy_escape(this->m_curl, input.c_str(), std::strlen(input.c_str()));

            if (escapedString != nullptr) {
                std::string output = escapedString;
                curl_free(escapedString);

                return output;
            }

            return {};
        }

        std::string urlDecode(const std::string &input) {
            auto unescapedString = curl_easy_unescape(this->m_curl, input.c_str(), std::strlen(input.c_str()), nullptr);

            if (unescapedString != nullptr) {
                std::string output = unescapedString;
                curl_free(unescapedString);

                return output;
            }

            return {};
        }

    protected:
        void setDefaultConfig();

        template<typename T>
        Result<T> executeImpl(std::vector<u8> &data) {
            curl_easy_setopt(this->m_curl, CURLOPT_URL, this->m_url.c_str());
            curl_easy_setopt(this->m_curl, CURLOPT_CUSTOMREQUEST, this->m_method.c_str());

            setDefaultConfig();

            if (!this->m_body.empty()) {
                curl_easy_setopt(this->m_curl, CURLOPT_POSTFIELDS, this->m_body.c_str());
            }

            curl_slist *headers = nullptr;
            headers = curl_slist_append(headers, "Cache-Control: no-cache");
            ON_SCOPE_EXIT { curl_slist_free_all(headers); };

            for (auto &[key, value] : this->m_headers) {
                std::string header = hex::format("{}: {}", key, value);
                headers = curl_slist_append(headers, header.c_str());
            }
            curl_easy_setopt(this->m_curl, CURLOPT_HTTPHEADER, headers);

            {
                std::scoped_lock lock(this->m_transmissionMutex);

                auto result = curl_easy_perform(this->m_curl);
                if (result != CURLE_OK){
                    char *url = nullptr;
                    curl_easy_getinfo(this->m_curl, CURLINFO_EFFECTIVE_URL, &url);
                    log::error("Http request '{0} {1}' failed with error {2}: '{3}'", this->m_method, url, u32(result), curl_easy_strerror(result));
                    if (!HttpRequest::s_proxyUrl.empty()){
                        log::info("A custom proxy '{0}' is in use. Is it working correctly?", HttpRequest::s_proxyUrl);
                    }

                    return { };
                }
            }

            long statusCode = 0;
            curl_easy_getinfo(this->m_curl, CURLINFO_RESPONSE_CODE, &statusCode);

            return Result<T>(statusCode, { data.begin(), data.end() });
        }

        [[maybe_unused]] static CURLcode sslCtxFunction(CURL *ctx, void *sslctx, void *userData);
        static size_t writeToVector(void *contents, size_t size, size_t nmemb, void *userdata);
        static size_t writeToFile(void *contents, size_t size, size_t nmemb, void *userdata);
        static int progressCallback(void *contents, curl_off_t dlTotal, curl_off_t dlNow, curl_off_t ulTotal, curl_off_t ulNow);

    private:
        CURL *m_curl;

        std::mutex m_transmissionMutex;

        std::string m_method;
        std::string m_url;
        std::string m_body;
        std::map<std::string, std::string> m_headers;
        u32 m_timeout = 1000;

        std::atomic<float> m_progress = 0.0F;
        std::atomic<bool> m_canceled = false;

        [[maybe_unused]] std::unique_ptr<mbedtls_x509_crt> m_caCert;
        static std::string s_caCertData, s_proxyUrl;
    };

}