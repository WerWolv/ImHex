#if !defined(OS_WEB)

#include <hex/helpers/http_requests.hpp>
#include <curl/curl.h>

namespace hex {

    namespace {

        std::string s_proxyUrl;
        bool s_proxyState;

    }

    int progressCallback(void *contents, curl_off_t dlTotal, curl_off_t dlNow, curl_off_t ulTotal, curl_off_t ulNow) {
        auto &request = *static_cast<HttpRequest *>(contents);

        if (dlTotal > 0)
            request.setProgress(float(dlNow) / dlTotal);
        else if (ulTotal > 0)
            request.setProgress(float(ulNow) / ulTotal);
        else
            request.setProgress(0.0F);

        return request.isCanceled() ? CURLE_ABORTED_BY_CALLBACK : CURLE_OK;
    }

    HttpRequest::HttpRequest(std::string method, std::string url) : m_method(std::move(method)), m_url(std::move(url)) {
        AT_FIRST_TIME {
            curl_global_init(CURL_GLOBAL_ALL);
        };

        AT_FINAL_CLEANUP {
            curl_global_cleanup();
        };

        m_curl = curl_easy_init();
    }

    HttpRequest::~HttpRequest() {
        curl_easy_cleanup(m_curl);
    }

    HttpRequest::HttpRequest(HttpRequest &&other) noexcept {
        m_curl = other.m_curl;
        other.m_curl = nullptr;

        m_method = std::move(other.m_method);
        m_url = std::move(other.m_url);
        m_headers = std::move(other.m_headers);
        m_body = std::move(other.m_body);
    }

    HttpRequest& HttpRequest::operator=(HttpRequest &&other) noexcept {
        m_curl = other.m_curl;
        other.m_curl = nullptr;

        m_method = std::move(other.m_method);
        m_url = std::move(other.m_url);
        m_headers = std::move(other.m_headers);
        m_body = std::move(other.m_body);
        m_timeout = other.m_timeout;

        return *this;
    }

    void HttpRequest::setDefaultConfig() {
        curl_easy_setopt(m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
        curl_easy_setopt(m_curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
        curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "ImHex/1.0");
        curl_easy_setopt(m_curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(m_curl, CURLOPT_TIMEOUT_MS, 0L);
        curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT_MS, m_timeout);
        curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, progressCallback);

        if (s_proxyState)
            curl_easy_setopt(m_curl, CURLOPT_PROXY, s_proxyUrl.c_str());
    }

    std::future<HttpRequest::Result<std::vector<u8>>> HttpRequest::downloadFile() {
        return std::async(std::launch::async, [this] {
            std::vector<u8> response;

            curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, writeToVector);
            curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);

            return this->executeImpl<std::vector<u8>>(response);
        });
    }



    void HttpRequest::setProxyUrl(std::string proxy) {
        s_proxyUrl = std::move(proxy);
    }

    void HttpRequest::setProxyState(bool enabled) {
        s_proxyState = enabled;
    }

    void HttpRequest::checkProxyErrors() {
        if (s_proxyState && !s_proxyUrl.empty()){
            log::info("A custom proxy '{0}' is in use. Is it working correctly?", s_proxyUrl);
        }
    }

    namespace impl {

        void setWriteFunctions(CURL *curl, wolv::io::File &file) {
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpRequest::writeToFile);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
        }

        void setWriteFunctions(CURL *curl, std::vector<u8> &data) {
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpRequest::writeToVector);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        }

        void setupFileUpload(CURL *curl, const wolv::io::File &file, const std::string &fileName, const std::string &mimeName) {
            curl_mime *mime     = curl_mime_init(curl);
            curl_mimepart *part = curl_mime_addpart(mime);


            curl_mime_data_cb(part, file.getSize(),
                [](char *buffer, size_t size, size_t nitems, void *arg) -> size_t {
                    auto handle = static_cast<FILE*>(arg);

                    return fread(buffer, size, nitems, handle);
                },
                [](void *arg, curl_off_t offset, int origin) -> int {
                    auto handle = static_cast<FILE*>(arg);

                    if (fseek(handle, offset, origin) != 0)
                        return CURL_SEEKFUNC_CANTSEEK;
                    else
                        return CURL_SEEKFUNC_OK;
                },
                [](void *arg) {
                    auto handle = static_cast<FILE*>(arg);

                    fclose(handle);
                },
                file.getHandle());
            curl_mime_filename(part, fileName.c_str());
            curl_mime_name(part, mimeName.c_str());

            curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        }

        void setupFileUpload(CURL *curl, const std::vector<u8> &data, const std::fs::path &fileName, const std::string &mimeName) {
            curl_mime *mime     = curl_mime_init(curl);
            curl_mimepart *part = curl_mime_addpart(mime);

            curl_mime_data(part, reinterpret_cast<const char *>(data.data()), data.size());
            auto fileNameStr = wolv::util::toUTF8String(fileName.filename());
            curl_mime_filename(part, fileNameStr.c_str());
            curl_mime_name(part, mimeName.c_str());

            curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        }

        int executeCurl(CURL *curl, const std::string &url, const std::string &method, const std::string &body, std::map<std::string, std::string> &headers) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());

            if (!body.empty()) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            }

            curl_slist *headersList = nullptr;
            headersList = curl_slist_append(headersList, "Cache-Control: no-cache");
            ON_SCOPE_EXIT { curl_slist_free_all(headersList); };

            for (auto &[key, value] : headers) {
                std::string header = fmt::format("{}: {}", key, value);
                headersList = curl_slist_append(headersList, header.c_str());
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headersList);

            auto result = curl_easy_perform(curl);
            if (result != CURLE_OK){
                return result;
            }

            return 0;
        }

        long getStatusCode(CURL *curl) {
            long statusCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);

            return statusCode;
        }

        std::string getStatusText(int result) {
            return curl_easy_strerror(CURLcode(result));
        }

    }


}


#endif