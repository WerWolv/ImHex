#pragma once

#include <hex.hpp>

#include <chrono>

#include <fmt/core.h>
#include <fmt/color.h>
#include <fmt/chrono.h>

namespace hex::log {

    FILE *getDestination();
    bool isRedirected();

    namespace {

        [[maybe_unused]] void printPrefix(FILE *dest, const fmt::text_style &ts, const std::string &level) {
            const auto now = fmt::localtime(std::chrono::system_clock::now());

            fmt::print(dest, "[{0:%H:%M:%S}] ", now);

            if (isRedirected())
                fmt::print(dest, "{0} ", level);
            else
                fmt::print(dest, ts, "{0} ", level);

            fmt::print(dest, "[{0}] ", IMHEX_PROJECT_NAME);
        }

        template<typename... T>
        [[maybe_unused]] void print(const fmt::text_style &ts, const std::string &level, const std::string &fmt, auto... args) {
            auto dest = getDestination();

            printPrefix(dest, ts, level);
            fmt::print(dest, fmt::runtime(fmt), args...);
            fmt::print(dest, "\n");
        }

    }

    [[maybe_unused]] void debug(const std::string &fmt, auto &&...args) {
#if defined(DEBUG)
        hex::log::print(fg(fmt::color::light_green) | fmt::emphasis::bold, "[DEBUG]", fmt, args...);
#else
        hex::unused(fmt, args...);
#endif
    }

    [[maybe_unused]] void info(const std::string &fmt, auto &&...args) {
        hex::log::print(fg(fmt::color::cadet_blue) | fmt::emphasis::bold, "[INFO] ", fmt, args...);
    }

    [[maybe_unused]] void warn(const std::string &fmt, auto &&...args) {
        hex::log::print(fg(fmt::color::orange) | fmt::emphasis::bold, "[WARN] ", fmt, args...);
    }

    [[maybe_unused]] void error(const std::string &fmt, auto &&...args) {
        hex::log::print(fg(fmt::color::red) | fmt::emphasis::bold, "[ERROR]", fmt, args...);
    }

    [[maybe_unused]] void fatal(const std::string &fmt, auto &&...args) {
        hex::log::print(fg(fmt::color::purple) | fmt::emphasis::bold, "[FATAL]", fmt, args...);
    }

    [[maybe_unused]] void redirectToFile();

}