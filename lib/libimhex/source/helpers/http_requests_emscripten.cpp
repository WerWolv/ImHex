#if defined(OS_WEB)

#include <hex/helpers/http_requests.hpp>

#include <fmt/format.h>

namespace hex {

    std::string HttpRequest::StatusCode::toString() const {
        return std::visit(wolv::util::overloaded {
            [](BackendStatus status) -> std::string {
                return "fetch() error " + std::to_string(u32(status));
            },
            [](HttpStatus status) -> std::string {
                return fmt::format("HTTP {}", u32(status));
            },
            [](auto) -> std::string {
                return "";
            }
        }, *this);
    }

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
}

#endif