#if defined(OS_WEB)

#include <hex/helpers/http_requests.hpp>

namespace hex {

    HttpRequest::HttpRequest(std::string method, std::string url) : m_method(std::move(method)), m_url(std::move(url)) {
        emscripten_fetch_attr_init(&m_attr);
    }

    HttpRequest::HttpRequest(HttpRequest &&other) noexcept {
        m_attr = other.m_attr;

        m_method = std::move(other.m_method);
        m_url = std::move(other.m_url);
        m_headers = std::move(other.m_headers);
        m_body = std::move(other.m_body);
    }

    HttpRequest& HttpRequest::operator=(HttpRequest &&other) noexcept {
        m_attr = other.m_attr;

        m_method = std::move(other.m_method);
        m_url = std::move(other.m_url);
        m_headers = std::move(other.m_headers);
        m_body = std::move(other.m_body);

        return *this;
    }

    HttpRequest::~HttpRequest() { }

    void HttpRequest::setDefaultConfig() { }

    std::future<HttpRequest::Result<std::vector<u8>>> HttpRequest::downloadFile() {
        return std::async(std::launch::async, [this] {
            std::vector<u8> response;

            return this->executeImpl<std::vector<u8>>(response);
        });
    }

    void HttpRequest::setProxyUrl(std::string proxy) {
        std::ignore = proxy;
    }

    void HttpRequest::setProxyState(bool state) {
        std::ignore = state;
    }

    void HttpRequest::checkProxyErrors() { }

    int HttpRequest::progressCallback(void *contents, curl_off_t dlTotal, curl_off_t dlNow, curl_off_t ulTotal, curl_off_t ulNow) {
        std::ignore = contents;
        std::ignore = dlTotal;
        std::ignore = dlNow;
        std::ignore = ulTotal;
        std::ignore = ulNow;
        return -1;
    }
}

#endif