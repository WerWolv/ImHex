#pragma once

#include <hex.hpp>
#include <span>
#include <string>
#include <vector>

namespace hex::clipboard {

    void init();
    void setBinaryData(std::span<const u8> data);
    [[nodiscard]] std::vector<u8> getBinaryData();
    void setTextData(const std::string &string);
    [[nodiscard]] std::string getTextData();

}
