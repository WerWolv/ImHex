#pragma once

#include <fmt/core.h>
#include <fmt/color.h>

namespace hex::log {

    void debug(const std::string &fmt, auto ... args) {
        #if defined(DEBUG)
            fmt::print(fg(fmt::color::green_yellow) | fmt::emphasis::bold, "[DEBUG] ");
            fmt::print(fmt::runtime(fmt), args...);
            fmt::print("\n");
        #endif
    }

    void info(const std::string &fmt, auto ... args) {
        fmt::print(fg(fmt::color::cadet_blue) | fmt::emphasis::bold, "[INFO]  ");
        fmt::print(fmt::runtime(fmt), args...);
        fmt::print("\n");
    }

    void warn(const std::string &fmt, auto ... args) {
        fmt::print(fg(fmt::color::orange) | fmt::emphasis::bold, "[WARN]  ");
        fmt::print(fmt::runtime(fmt), args...);
        fmt::print("\n");
    }

    void error(const std::string &fmt, auto ... args) {
        fmt::print(fg(fmt::color::red) | fmt::emphasis::bold, "[ERROR] ");
        fmt::print(fmt::runtime(fmt), args...);
        fmt::print("\n");
    }

    void fatal(const std::string &fmt, auto ... args) {
        fmt::print(fg(fmt::color::purple) | fmt::emphasis::bold, "[FATAL] ");
        fmt::print(fmt::runtime(fmt), args...);
        fmt::print("\n");
    }

}