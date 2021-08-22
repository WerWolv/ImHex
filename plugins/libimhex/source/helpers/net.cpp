#include <hex/helpers/net.hpp>

#include <hex/helpers/utils.hpp>

#include <filesystem>

#include <mbedtls/ssl.h>
#include <mbedtls/x509.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/error.h>

#include <hex/resources.hpp>

namespace hex {

    Net::Net() {
        curl_global_sslset(CURLSSLBACKEND_MBEDTLS, nullptr, nullptr);
        curl_global_init(CURL_GLOBAL_ALL);
        this->m_ctx = curl_easy_init();
    }

    Net::~Net() {
        curl_easy_cleanup(this->m_ctx);
        curl_global_cleanup();
    }

    static size_t writeToString(void *contents, size_t size, size_t nmemb, void *userp){
        static_cast<std::string*>(userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    static CURLcode sslCtxFunction(CURL *ctx, void *sslctx, void *userdata) {
        auto* cfg = static_cast<mbedtls_ssl_config*>(sslctx);

        static mbedtls_x509_crt crt;
        mbedtls_x509_crt_init(&crt);
        mbedtls_x509_crt_parse(&crt, cacert, cacert_size);

        mbedtls_ssl_conf_ca_chain(cfg, &crt, nullptr);

        return CURLE_OK;
    }

    static void setCommonSettings(CURL *ctx, std::string &response, std::string_view path, const std::map<std::string, std::string> &extraHeaders, const std::string &body) {
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Cache-Control: no-cache");

        if (!extraHeaders.empty())
            for (const auto &[key, value] : extraHeaders) {
                std::string entry = key;
                entry += ": ";
                entry += value;

                headers = curl_slist_append(headers, entry.c_str());
            }

        if (!body.empty())
            curl_easy_setopt(ctx, CURLOPT_POSTFIELDS, body.c_str());

        curl_easy_setopt(ctx, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
        curl_easy_setopt(ctx, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2 | CURL_SSLVERSION_MAX_TLSv1_2);
        curl_easy_setopt(ctx, CURLOPT_URL, path.data());
        curl_easy_setopt(ctx, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(ctx, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(ctx, CURLOPT_USERAGENT, "ImHex/1.0");
        curl_easy_setopt(ctx, CURLOPT_DEFAULT_PROTOCOL, "https");
        curl_easy_setopt(ctx, CURLOPT_WRITEFUNCTION, writeToString);
        curl_easy_setopt(ctx, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(ctx, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(ctx, CURLOPT_CAINFO, nullptr);
        curl_easy_setopt(ctx, CURLOPT_CAPATH, nullptr);
        curl_easy_setopt(ctx, CURLOPT_SSLCERTTYPE, "PEM");
        curl_easy_setopt(ctx, CURLOPT_SSL_CTX_FUNCTION, sslCtxFunction);
        curl_easy_setopt(ctx, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(ctx, CURLOPT_TIMEOUT_MS, 2000L);
        curl_easy_setopt(ctx, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
        curl_easy_setopt(ctx, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(ctx, CURLOPT_NOSIGNAL, 1L);
    }

    Response<std::string> Net::getString(std::string_view url) {
        std::string response;

        curl_easy_setopt(this->m_ctx, CURLOPT_CUSTOMREQUEST, "GET");
        setCommonSettings(this->m_ctx, response, url, {}, "");

        CURLcode result = curl_easy_perform(this->m_ctx);

        u32 responseCode = 0;
        curl_easy_getinfo(this->m_ctx, CURLINFO_RESPONSE_CODE, &responseCode);

        if (result != CURLE_OK)
            return Response<std::string>{ responseCode, "" };
        else
            return Response<std::string>{ responseCode, response };
    }

    Response<nlohmann::json> Net::getJson(std::string_view url) {
        std::string response;

        curl_easy_setopt(this->m_ctx, CURLOPT_CUSTOMREQUEST, "GET");
        setCommonSettings(this->m_ctx, response, url, {}, "");

        CURLcode result = curl_easy_perform(this->m_ctx);

        u32 responseCode = 0;
        curl_easy_getinfo(this->m_ctx, CURLINFO_RESPONSE_CODE, &responseCode);

        if (result != CURLE_OK)
            return Response<nlohmann::json>{ responseCode, { } };
        else
            return Response<nlohmann::json>{ responseCode, nlohmann::json::parse(response) };
    }

}