#pragma once

#include <chrono>

#include <fmt/core.h>
#include <fmt/color.h>
#include <fmt/chrono.h>

namespace hex::log {

    FILE *getDestination();
    bool isRedirected();

    namespace {

        void printPrefix(FILE *dest, const fmt::text_style &ts, const std::string &level) {
            const auto now = fmt::localtime(std::chrono::system_clock::now());

            fmt::print(dest, "[{0:%H:%M:%S}] ", now);

            if (isRedirected())
                fmt::print(dest, "{0} ", level);
            else
                fmt::print(dest, ts, "{0} ", level);

            fmt::print(dest, "[{0}] ", IMHEX_PROJECT_NAME);
        }

        template<typename... T>
        void print(const fmt::text_style &ts, const std::string &level, const std::string &fmt, auto... args) {
            auto dest = getDestination();

            printPrefix(dest, ts, level);
            fmt::print(dest, fmt::runtime(fmt), args...);
            fmt::print(dest, "\n");
        }

    }

    void debug(const std::string &fmt, auto &&...args) {
#if defined(DEBUG)
        log::print(fg(fmt::color::green_yellow) | fmt::emphasis::bold, "[DEBUG]", fmt, args...);
#else
        hex::unused(fmt, args...);
#endif
    }

    void info(const std::string &fmt, auto &&...args) {
        log::print(fg(fmt::color::cadet_blue) | fmt::emphasis::bold, "[INFO] ", fmt, args...);
    }

    void warn(const std::string &fmt, auto &&...args) {
        log::print(fg(fmt::color::orange) | fmt::emphasis::bold, "[WARN] ", fmt, args...);
    }

    void error(const std::string &fmt, auto &&...args) {
        log::print(fg(fmt::color::red) | fmt::emphasis::bold, "[ERROR]", fmt, args...);
    }

    void fatal(const std::string &fmt, auto &&...args) {
        log::print(fg(fmt::color::purple) | fmt::emphasis::bold, "[FATAL]", fmt, args...);
    }

    void redirectToFile();

}