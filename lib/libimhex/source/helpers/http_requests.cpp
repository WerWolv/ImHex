#include <hex/helpers/http_requests.hpp>

namespace hex {

    size_t HttpRequest::writeToVector(void *contents, size_t size, size_t nmemb, void *userdata) {
        auto &response = *reinterpret_cast<std::vector<u8>*>(userdata);
        auto startSize = response.size();

        response.resize(startSize + size * nmemb);
        std::memcpy(response.data() + startSize, contents, size * nmemb);

        return size * nmemb;
    }

    size_t HttpRequest::writeToFile(void *contents, size_t size, size_t nmemb, void *userdata) {
        auto &file = *reinterpret_cast<wolv::io::File*>(userdata);

        file.writeBuffer(reinterpret_cast<const u8*>(contents), size * nmemb);

        return size * nmemb;
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