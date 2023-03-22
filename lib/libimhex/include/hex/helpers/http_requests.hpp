#pragma once

#include <future>

#include <curl/curl.h>

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
            explicit ResultBase(u32 statusCode) : m_statusCode(statusCode) { }

            [[nodiscard]] u32 getStatusCode() const {
                return this->m_statusCode;
            }

            [[nodiscard]] bool isSuccess() const {
                return this->getStatusCode() == 200;
            }

        private:
            u32 m_statusCode;
        };

        template<typename T>
        class Result : public ResultBase {
        public:
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

        static void setCaCert(std::string data) {
            HttpRequest::s_caCertData = std::move(data);
        }

        void addHeader(std::string key, std::string value) {
            this->m_headers[std::move(key)] = std::move(value);
        }

        void setBody(std::string body) {
            this->m_body = std::move(body);
        }

        template<typename T>
        std::future<Result<T>> downloadFile(const std::fs::path &path) {
            return std::async(std::launch::async, [this, path] {
                T response;

                wolv::io::File file(path, wolv::io::File::Mode::Create);
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, [](void *contents, size_t size, size_t nmemb, void *userdata){
                    auto &file = *reinterpret_cast<wolv::io::File*>(userdata);

                    file.write(reinterpret_cast<const u8*>(contents), size * nmemb);

                    return size * nmemb;
                });
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &file);

                this->executeImpl<T>(response);

                return Result<T>(200, std::move(response));
            });
        }

        std::future<Result<std::vector<u8>>> downloadFile() {
            return std::async(std::launch::async, [this] {
                std::vector<u8> response;

                curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, [](void *contents, size_t size, size_t nmemb, void *userdata){
                    auto &response = *reinterpret_cast<std::vector<u8>*>(userdata);

                    response.insert(response.end(), reinterpret_cast<const u8*>(contents), reinterpret_cast<const u8*>(contents) + size * nmemb);

                    return size * nmemb;
                });
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &response);

                return this->executeImpl<std::vector<u8>>(response);
            });
        }

        template<typename T>
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

                T responseData;
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, writeToContainer<T>);
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &responseData);

                return this->executeImpl<T>(responseData);
            });
        }
        template<typename T>
        std::future<Result<T>> uploadFile(std::vector<u8> data, const std::string &mimeName = "filename") {
            return std::async(std::launch::async, [this, data = std::move(data)]{
                curl_mime *mime     = curl_mime_init(this->m_curl);
                curl_mimepart *part = curl_mime_addpart(mime);

                curl_mime_data(part, reinterpret_cast<const char *>(data.data()), data.size());
                curl_mime_filename(part, "data.bin");
                curl_mime_name(part, mimeName.c_str());

                curl_easy_setopt(this->m_curl, CURLOPT_MIMEPOST, mime);

                T responseData;
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, writeToContainer<T>);
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &responseData);

                return this->executeImpl<T>(responseData);
            });
        }

        template<typename T>
        std::future<Result<T>> execute() {
            return std::async(std::launch::async, [this] {
                T data;

                curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, writeToContainer<T>);
                curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &data);

                return this->executeImpl<T>(data);
            });
        }

    protected:
        template<typename T>
        Result<T> executeImpl(T &data) {
            curl_easy_setopt(this->m_curl, CURLOPT_URL, this->m_url.c_str());
            curl_easy_setopt(this->m_curl, CURLOPT_CUSTOMREQUEST, this->m_method.c_str());

            curl_easy_setopt(this->m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
            curl_easy_setopt(this->m_curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
            curl_easy_setopt(this->m_curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(this->m_curl, CURLOPT_USERAGENT, "ImHex/1.0");
            curl_easy_setopt(this->m_curl, CURLOPT_DEFAULT_PROTOCOL, "https");
            curl_easy_setopt(this->m_curl, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(this->m_curl, CURLOPT_SSL_VERIFYHOST, 2L);
            curl_easy_setopt(this->m_curl, CURLOPT_TIMEOUT_MS, 0L);
            curl_easy_setopt(this->m_curl, CURLOPT_CONNECTTIMEOUT_MS, 10000);
            curl_easy_setopt(this->m_curl, CURLOPT_NOSIGNAL, 1L);

            #if defined(IMHEX_USE_BUNDLED_CA)
                curl_easy_setopt(this->m_curl, CURLOPT_CAINFO, nullptr);
                curl_easy_setopt(this->m_curl, CURLOPT_CAPATH, nullptr);
                curl_easy_setopt(this->m_curl, CURLOPT_SSLCERTTYPE, "PEM");
                curl_easy_setopt(this->m_curl, CURLOPT_SSL_CTX_FUNCTION, sslCtxFunction);
                curl_easy_setopt(this->m_curl, CURLOPT_SSL_CTX_DATA, &this->m_caCert);
            #endif

            if (!this->m_body.empty()) {
                curl_easy_setopt(this->m_curl, CURLOPT_POSTFIELDS, this->m_body.c_str());
            }

            curl_slist *headers = nullptr;
            headers = curl_slist_append(headers, "Cache-Control: no-cache");

            for (auto &[key, value] : this->m_headers) {
                std::string header = key + ": " + value;
                headers = curl_slist_append(headers, header.c_str());
            }
            curl_easy_setopt(this->m_curl, CURLOPT_HTTPHEADER, headers);

            auto result = curl_easy_perform(this->m_curl);
            printf("curl result: %d\n", result);

            u32 statusCode = 0;
            curl_easy_getinfo(this->m_curl, CURLINFO_RESPONSE_CODE, &statusCode);

            return Result<T>(statusCode, std::move(data));
        }

        [[maybe_unused]]
        static CURLcode sslCtxFunction(CURL *ctx, void *sslctx, void *userData) {
            hex::unused(ctx, userData);

            auto *cfg = static_cast<mbedtls_ssl_config *>(sslctx);

            auto crt = static_cast<mbedtls_x509_crt*>(userData);
            mbedtls_x509_crt_init(crt);

            mbedtls_x509_crt_parse(crt, reinterpret_cast<const u8 *>(HttpRequest::s_caCertData.data()), HttpRequest::s_caCertData.size());

            mbedtls_ssl_conf_ca_chain(cfg, crt, nullptr);

            return CURLE_OK;
        }

        template<typename T>
        static size_t writeToContainer(void *contents, size_t size, size_t nmemb, void *userdata) {
            auto &response = *reinterpret_cast<T*>(userdata);
            auto startSize = response.size();

            response.resize(startSize + size * nmemb);
            std::memcpy(response.data() + startSize, contents, size * nmemb);

            return size * nmemb;
        }

    private:
        CURL *m_curl;

        std::string m_method;
        std::string m_url;
        std::string m_body;
        std::map<std::string, std::string> m_headers;

        mbedtls_x509_crt m_caCert;
        static inline std::string s_caCertData;
    };

}