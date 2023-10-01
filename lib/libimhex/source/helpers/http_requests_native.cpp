#if !defined(OS_EMSCRIPTEN)

#include <hex/helpers/http_requests.hpp>

namespace hex {

    namespace {
        std::string s_proxyUrl;
    }

    HttpRequest::HttpRequest(std::string method, std::string url) : m_method(std::move(method)), m_url(std::move(url)) {
        AT_FIRST_TIME {
            curl_global_init(CURL_GLOBAL_ALL);
        };

        AT_FINAL_CLEANUP {
            curl_global_cleanup();
        };

        this->m_curl = curl_easy_init();
    }

    HttpRequest::~HttpRequest() {
        curl_easy_cleanup(this->m_curl);
    }

    HttpRequest::HttpRequest(HttpRequest &&other) noexcept {
        this->m_curl = other.m_curl;
        other.m_curl = nullptr;

        this->m_method = std::move(other.m_method);
        this->m_url = std::move(other.m_url);
        this->m_headers = std::move(other.m_headers);
        this->m_body = std::move(other.m_body);
    }

    HttpRequest& HttpRequest::operator=(HttpRequest &&other) noexcept {
        this->m_curl = other.m_curl;
        other.m_curl = nullptr;

        this->m_method = std::move(other.m_method);
        this->m_url = std::move(other.m_url);
        this->m_headers = std::move(other.m_headers);
        this->m_body = std::move(other.m_body);

        return *this;
    }

    void HttpRequest::setDefaultConfig() {
        curl_easy_setopt(this->m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
        curl_easy_setopt(this->m_curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
        curl_easy_setopt(this->m_curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(this->m_curl, CURLOPT_USERAGENT, "ImHex/1.0");
        curl_easy_setopt(this->m_curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        curl_easy_setopt(this->m_curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(this->m_curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(this->m_curl, CURLOPT_TIMEOUT_MS, 0L);
        curl_easy_setopt(this->m_curl, CURLOPT_CONNECTTIMEOUT_MS, this->m_timeout);
        curl_easy_setopt(this->m_curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(this->m_curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(this->m_curl, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(this->m_curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(this->m_curl, CURLOPT_PROXY, s_proxyUrl.c_str());
    }

    std::future<HttpRequest::Result<std::vector<u8>>> HttpRequest::downloadFile() {
        return std::async(std::launch::async, [this] {
            std::vector<u8> response;

            curl_easy_setopt(this->m_curl, CURLOPT_WRITEFUNCTION, writeToVector);
            curl_easy_setopt(this->m_curl, CURLOPT_WRITEDATA, &response);

            return this->executeImpl<std::vector<u8>>(response);
        });
    }



    void HttpRequest::setProxy(std::string proxy) {
        s_proxyUrl = std::move(proxy);
    }

    void HttpRequest::checkProxyErrors() {
        if (!s_proxyUrl.empty()){
            log::info("A custom proxy '{0}' is in use. Is it working correctly?", s_proxyUrl);
        }
    }

    int HttpRequest::progressCallback(void *contents, curl_off_t dlTotal, curl_off_t dlNow, curl_off_t ulTotal, curl_off_t ulNow) {
        auto &request = *static_cast<HttpRequest *>(contents);

        if (dlTotal > 0)
            request.m_progress = float(dlNow) / dlTotal;
        else if (ulTotal > 0)
            request.m_progress = float(ulNow) / ulTotal;
        else
            request.m_progress = 0.0F;

        return request.m_canceled ? CURLE_ABORTED_BY_CALLBACK : CURLE_OK;
    }

}


#endif