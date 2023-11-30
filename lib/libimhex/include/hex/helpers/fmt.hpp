#pragma once

#include <string_view>
#include <fmt/format.h>

namespace hex {

    template<typename... Args>
    std::string format(std::string_view format, Args... args) {
        return fmt::format(fmt::runtime(format), args...);
    }

}