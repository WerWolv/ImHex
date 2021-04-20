#pragma once

#include <fmt/core.h>
#include <fmt/color.h>

namespace hex::log {

    void debug(std::string_view fmt, auto ... args) {
        #if defined(DEBUG)
            fmt::print(fg(fmt::color::green_yellow) | fmt::emphasis::bold, "[DEBUG] ");
            fmt::print(fmt, args...);
            fmt::print("\n");
        #endif
    }

    void info(std::string_view fmt, auto ... args) {
        fmt::print(fg(fmt::color::cadet_blue) | fmt::emphasis::bold, "[INFO]  ");
        fmt::print(fmt, args...);
        fmt::print("\n");
    }

    void warn(std::string_view fmt, auto ... args) {
        fmt::print(fg(fmt::color::light_golden_rod_yellow) | fmt::emphasis::bold, "[WARN]  ");
        fmt::print(fmt, args...);
        fmt::print("\n");
    }

    void error(std::string_view fmt, auto ... args) {
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "[ERROR] ");
        fmt::print(fmt, args...);
        fmt::print("\n");
    }

    void fatal(std::string_view fmt, auto ... args) {
        fmt::print(fg(fmt::color::purple) | fmt::emphasis::bold, "[FATAL] ");
        fmt::print(fmt, args...);
        fmt::print("\n");
    }

}