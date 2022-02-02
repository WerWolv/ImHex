#pragma once

#include <chrono>

#include <fmt/core.h>
#include <fmt/color.h>
#include <fmt/chrono.h>

namespace hex::log {

    namespace {

        void printPrefix() {
            const auto now = fmt::localtime(std::chrono::system_clock::now());
            fmt::print("[{0:%H:%M:%S}] [{1}] ", now, IMHEX_PROJECT_NAME);
        }

    }

    FILE *getDestination();
    bool isRedirected();

    template<typename... T>
    void print(fmt::format_string<T...> fmt, T &&...args) {
        fmt::print(getDestination(), fmt, args...);
    }

    template<typename S, typename... Args>
    void print(const fmt::text_style &ts, const S &fmt, const Args &...args) {
        printPrefix();
        if (isRedirected())
            fmt::print(getDestination(), fmt::runtime(fmt), args...);
        else
            fmt::print(getDestination(), ts, fmt, args...);
    }

    void debug(const std::string &fmt, auto... args) {
#if defined(DEBUG)
        log::print(fg(fmt::color::green_yellow) | fmt::emphasis::bold, "[DEBUG] ");
        log::print(fmt::runtime(fmt), args...);
        log::print("\n");
#endif
    }

    void info(const std::string &fmt, auto... args) {
        log::print(fg(fmt::color::cadet_blue) | fmt::emphasis::bold, "[INFO]  ");
        log::print(fmt::runtime(fmt), args...);
        log::print("\n");
    }

    void warn(const std::string &fmt, auto... args) {
        log::print(fg(fmt::color::orange) | fmt::emphasis::bold, "[WARN]  ");
        log::print(fmt::runtime(fmt), args...);
        log::print("\n");
    }

    void error(const std::string &fmt, auto... args) {
        log::print(fg(fmt::color::red) | fmt::emphasis::bold, "[ERROR] ");
        log::print(fmt::runtime(fmt), args...);
        log::print("\n");
    }

    void fatal(const std::string &fmt, auto... args) {
        log::print(fg(fmt::color::purple) | fmt::emphasis::bold, "[FATAL] ");
        log::print(fmt::runtime(fmt), args...);
        log::print("\n");
    }

    void redirectToFile();

}