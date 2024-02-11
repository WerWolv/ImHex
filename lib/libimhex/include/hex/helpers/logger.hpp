#pragma once

#include <hex.hpp>

#include <mutex>

#include <fmt/core.h>
#include <fmt/color.h>

#include <wolv/io/file.hpp>

namespace hex::log {

    namespace impl {

        FILE *getDestination();
        wolv::io::File& getFile();
        bool isRedirected();
        [[maybe_unused]] void redirectToFile();
        [[maybe_unused]] void enableColorPrinting();

        [[nodiscard]] std::mutex& getLoggerMutex();
        [[nodiscard]] bool isLoggingSuspended();

        struct LogEntry {
            std::string project;
            std::string level;
            std::string message;
        };

        std::vector<LogEntry>& getLogEntries();
        void addLogEntry(std::string_view project, std::string_view level, std::string_view message);

        [[maybe_unused]] void printPrefix(FILE *dest, const fmt::text_style &ts, const std::string &level, const char *projectName);

        [[maybe_unused]] void print(const fmt::text_style &ts, const std::string &level, const std::string &fmt, auto && ... args) {
            if (isLoggingSuspended()) [[unlikely]]
                return;

            std::scoped_lock lock(getLoggerMutex());

            auto dest = getDestination();
            printPrefix(dest, ts, level, IMHEX_PROJECT_NAME);

            auto message = fmt::format(fmt::runtime(fmt), args...);
            fmt::print(dest, "{}\n", message);
            fflush(dest);

            addLogEntry(IMHEX_PROJECT_NAME, level, std::move(message));
        }

    }

    void suspendLogging();
    void resumeLogging();

    [[maybe_unused]] void debug(const std::string &fmt, auto && ... args) {
        #if defined(DEBUG)
            hex::log::impl::print(fg(fmt::color::light_green) | fmt::emphasis::bold, "[DEBUG]", fmt, args...);
        #else
            impl::addLogEntry(IMHEX_PROJECT_NAME, "[DEBUG]", fmt::format(fmt::runtime(fmt), args...));
        #endif
    }

    [[maybe_unused]] void info(const std::string &fmt, auto && ... args) {
        hex::log::impl::print(fg(fmt::color::cadet_blue) | fmt::emphasis::bold, "[INFO] ", fmt, args...);
    }

    [[maybe_unused]] void warn(const std::string &fmt, auto && ... args) {
        hex::log::impl::print(fg(fmt::color::orange) | fmt::emphasis::bold, "[WARN] ", fmt, args...);
    }

    [[maybe_unused]] void error(const std::string &fmt, auto && ... args) {
        hex::log::impl::print(fg(fmt::color::red) | fmt::emphasis::bold, "[ERROR]", fmt, args...);
    }

    [[maybe_unused]] void fatal(const std::string &fmt, auto && ... args) {
        hex::log::impl::print(fg(fmt::color::purple) | fmt::emphasis::bold, "[FATAL]", fmt, args...);
    }


    [[maybe_unused]] void print(const std::string &fmt, auto && ... args) {
        std::scoped_lock lock(impl::getLoggerMutex());

        auto dest = impl::getDestination();
        auto message = fmt::format(fmt::runtime(fmt), args...);
        fmt::print(dest, "{}", message);
        fflush(dest);
    }

    [[maybe_unused]] void println(const std::string &fmt, auto && ... args) {
        std::scoped_lock lock(impl::getLoggerMutex());

        auto dest = impl::getDestination();
        auto message = fmt::format(fmt::runtime(fmt), args...);
        fmt::print(dest, "{}\n", message);
        fflush(dest);
    }

}