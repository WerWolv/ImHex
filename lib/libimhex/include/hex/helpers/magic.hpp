#pragma once

#include <hex.hpp>

#include <hex/helpers/literals.hpp>

#include <string>
#include <vector>

namespace hex::prv {
    class Provider;
}

namespace hex::magic {

    using namespace hex::literals;

    bool compile();
    std::string getDescription(const std::vector<u8> &data);
    std::string getDescription(prv::Provider *provider, size_t size = 100_KiB);
    std::string getMIMEType(const std::vector<u8> &data);
    std::string getMIMEType(prv::Provider *provider, size_t size = 100_KiB);

    bool isValidMIMEType(const std::string &mimeType);

}