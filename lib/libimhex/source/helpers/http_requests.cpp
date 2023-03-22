#include <hex/helpers/http_requests.hpp>

namespace hex {

    std::string HttpRequest::s_caCertData;

    void HttpRequest::setDefaultConfig() {
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

            this->m_caCert = std::make_unique<mbedtls_x509_crt>();
            curl_easy_setopt(this->m_curl, CURLOPT_SSL_CTX_DATA, this->m_caCert.get());
        #endif
    }

}