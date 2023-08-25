#pragma once

#include <hex.hpp>

#include <chrono>
#include <mutex>

#include <fmt/core.h>
#include <fmt/color.h>
#include <fmt/chrono.h>

#include <wolv/io/file.hpp>

namespace hex::log {

    namespace impl {

        FILE *getDestination();
        wolv::io::File& getFile();
        bool isRedirected();
        [[maybe_unused]] void redirectToFile();

        extern std::mutex g_loggerMutex;

        struct LogEntry {
            std::string project;
            std::string level;
            std::string message;
        };

        std::vector<LogEntry>& getLogEntries();
    }

    namespace {


        [[maybe_unused]] void printPrefix(FILE *dest, const fmt::text_style &ts, const std::string &level) {
            const auto now = fmt::localtime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

            fmt::print(dest, "[{0:%H:%M:%S}] ", now);

            if (impl::isRedirected())
                fmt::print(dest, "{0} ", level);
            else
                fmt::print(dest, ts, "{0} ", level);

            fmt::print(dest, "[{0}] ", IMHEX_PROJECT_NAME);
        }

        template<typename... T>
        [[maybe_unused]] void print(const fmt::text_style &ts, const std::string &level, const std::string &fmt, auto... args) {
            std::scoped_lock lock(impl::g_loggerMutex);

            auto dest = impl::getDestination();
            printPrefix(dest, ts, level);

            auto message = fmt::format(fmt::runtime(fmt), args...);
            fmt::print(dest, "{}\n", message);
            fflush(dest);

            impl::getLogEntries().push_back({ IMHEX_PROJECT_NAME, level, std::move(message) });
        }

    }

    [[maybe_unused]] void debug(const std::string &fmt, auto &&...args) {
        #if defined(DEBUG)
            hex::log::print(fg(fmt::color::light_green) | fmt::emphasis::bold, "[DEBUG]", fmt, args...);
        #else
            impl::getLogEntries().push_back({ IMHEX_PROJECT_NAME, "[DEBUG]", fmt::format(fmt::runtime(fmt), args...) });
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

}