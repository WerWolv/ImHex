#pragma once

#include <string_view>
#include <fmt/format.h>
#include <fmt/chrono.h>

namespace hex {

    template<typename... Args>
    inline std::string format(std::string_view format, Args... args) {
        return fmt::format(fmt::runtime(format), args...);
    }

    template<typename... Args>
    inline void print(std::string_view format, Args... args) {
        fmt::print(fmt::runtime(format), args...);
    }

}