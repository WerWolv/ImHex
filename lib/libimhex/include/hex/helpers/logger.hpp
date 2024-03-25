#pragma once

#include <hex.hpp>

#include <fmt/core.h>
#include <fmt/color.h>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>

namespace hex::log {

    namespace impl {

        FILE *getDestination();
        wolv::io::File& getFile();
        bool isRedirected();
        [[maybe_unused]] void redirectToFile();
        [[maybe_unused]] void enableColorPrinting();

        [[nodiscard]] bool isLoggingSuspended();
        [[nodiscard]] bool isDebugLoggingEnabled();

        void lockLoggerMutex();
        void unlockLoggerMutex();

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

            lockLoggerMutex();
            ON_SCOPE_EXIT { unlockLoggerMutex(); };

            auto dest = getDestination();
            try {
                printPrefix(dest, ts, level, IMHEX_PROJECT_NAME);

                auto message = fmt::format(fmt::runtime(fmt), args...);
                fmt::print(dest, "{}\n", message);
                fflush(dest);

                addLogEntry(IMHEX_PROJECT_NAME, level, std::move(message));
            } catch (const std::exception&) { }
        }

        namespace color {

            fmt::color debug();
            fmt::color info();
            fmt::color warn();
            fmt::color error();
            fmt::color fatal();

        }

    }

    void suspendLogging();
    void resumeLogging();
    void enableDebugLogging();

    [[maybe_unused]] void debug(const std::string &fmt, auto && ... args) {
        if (impl::isDebugLoggingEnabled()) [[unlikely]] {
            hex::log::impl::print(fg(impl::color::debug()) | fmt::emphasis::bold, "[DEBUG]", fmt, args...);
        } else {
            impl::addLogEntry(IMHEX_PROJECT_NAME, "[DEBUG]", fmt::format(fmt::runtime(fmt), args...));
        }
    }

    [[maybe_unused]] void info(const std::string &fmt, auto && ... args) {
        hex::log::impl::print(fg(impl::color::info()) | fmt::emphasis::bold, "[INFO] ", fmt, args...);
    }

    [[maybe_unused]] void warn(const std::string &fmt, auto && ... args) {
        hex::log::impl::print(fg(impl::color::warn()) | fmt::emphasis::bold, "[WARN] ", fmt, args...);
    }

    [[maybe_unused]] void error(const std::string &fmt, auto && ... args) {
        hex::log::impl::print(fg(impl::color::error()) | fmt::emphasis::bold, "[ERROR]", fmt, args...);
    }

    [[maybe_unused]] void fatal(const std::string &fmt, auto && ... args) {
        hex::log::impl::print(fg(impl::color::fatal()) | fmt::emphasis::bold, "[FATAL]", fmt, args...);
    }

    [[maybe_unused]] void print(const std::string &fmt, auto && ... args) {
        impl::lockLoggerMutex();
        ON_SCOPE_EXIT { impl::unlockLoggerMutex(); };

        try {
            auto dest = impl::getDestination();
            auto message = fmt::format(fmt::runtime(fmt), args...);
            fmt::print(dest, "{}", message);
            fflush(dest);
        } catch (const std::exception&) { }
    }

    [[maybe_unused]] void println(const std::string &fmt, auto && ... args) {
        impl::lockLoggerMutex();
        ON_SCOPE_EXIT { impl::unlockLoggerMutex(); };

        try {
            auto dest = impl::getDestination();
            auto message = fmt::format(fmt::runtime(fmt), args...);
            fmt::print(dest, "{}\n", message);
            fflush(dest);
        } catch (const std::exception&) { }
    }

}
