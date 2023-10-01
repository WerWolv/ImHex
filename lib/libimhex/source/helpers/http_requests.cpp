#include <hex/helpers/http_requests.hpp>

#include <hex/helpers/crypto.hpp>

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

    std::string HttpRequest::urlEncode(const std::string &input) {
        std::string result;
        for (char c : input){
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
                result += c;
            else
                result += hex::format("%02X", c);;
        }
        return result;
    }

    std::string HttpRequest::urlDecode(const std::string &input) {
        std::string result;

        for (u32 i = 0; i < input.size(); i++){
            if (input[i] != '%'){
                if (input[i] == '+')
                    result += ' ';
                else
                    result += input[i];
            } else {
                const auto hex = crypt::decode16(input.substr(i + 1, 2));
                if (hex.empty())
                    return "";

                result += char(hex[0]);
                i += 2;
            }
        }
        return input;
    }

}