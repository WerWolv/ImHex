#pragma once

#include <string>

#include <wolv/types.hpp>

#include <hex.hpp>

EXPORT_MODULE namespace hex {
    [[nodiscard]] std::string formatPattern(const std::string& code, u32 tabSize);
    [[nodiscard]] std::string preprocessPattern(const std::string& code, u32 tabSize);
}
