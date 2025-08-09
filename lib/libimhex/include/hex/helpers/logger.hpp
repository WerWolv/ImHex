#pragma once

#include <hex.hpp>

#include <fmt/core.h>
#include <fmt/color.h>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>

EXPORT_MODULE namespace hex::log {

    namespace impl {

        [[nodiscard]] FILE *getDestination();
        [[nodiscard]] wolv::io::File& getFile();
        [[nodiscard]] bool isRedirected();
        [[maybe_unused]] void redirectToFile();
        [[maybe_unused]] void enableColorPrinting();

        [[nodiscard]] bool isLoggingSuspended();
        [[nodiscard]] bool isDebugLoggingEnabled();

        void lockLoggerMutex();
        void unlockLoggerMutex();

        struct LogEntry {
            std::string_view project;
            std::string_view level;
            std::string message;
        };

        const std::vector<LogEntry>& getLogEntries();
        void addLogEntry(std::string_view project, std::string_view level, std::string message);

        [[maybe_unused]] void printPrefix(FILE *dest, fmt::text_style ts, std::string_view level, std::string_view projectName);

        template<typename ... Args>
        [[maybe_unused]] void print(fmt::text_style ts, std::string_view level, fmt::format_string<Args...> fmt, Args && ... args) {
            if (isLoggingSuspended()) [[unlikely]]
                return;

            lockLoggerMutex();
            ON_SCOPE_EXIT { unlockLoggerMutex(); };

            auto dest = getDestination();
            try {
                printPrefix(dest, ts, level, IMHEX_PROJECT_NAME);

                auto message = fmt::format(fmt, std::forward<Args>(args)...);
                fmt::print(dest, "{}\n", message);
                std::fflush(dest);

                addLogEntry(IMHEX_PROJECT_NAME, level, std::move(message));
            } catch (const std::exception&) {
                /* Ignore any exceptions, we can't do anything anyway */
            }
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

    template<typename ... Args>
    [[maybe_unused]] void debug(fmt::format_string<Args...> fmt, Args && ... args) {
        if (impl::isDebugLoggingEnabled()) [[unlikely]] {
            impl::print(fg(impl::color::debug()) | fmt::emphasis::bold, "[DEBUG]", fmt, std::forward<Args>(args)...);
        } else {
            impl::addLogEntry(IMHEX_PROJECT_NAME, "[DEBUG]", fmt::format(fmt, std::forward<Args>(args)...));
        }
    }

    template<typename ... Args>
    [[maybe_unused]] void info(fmt::format_string<Args...> fmt, Args && ... args) {
        impl::print(fg(impl::color::info()) | fmt::emphasis::bold, "[INFO] ", fmt, std::forward<Args>(args)...);
    }

    template<typename ... Args>
    [[maybe_unused]] void warn(fmt::format_string<Args...> fmt, Args && ... args) {
        impl::print(fg(impl::color::warn()) | fmt::emphasis::bold, "[WARN] ", fmt, std::forward<Args>(args)...);
    }

    template<typename ... Args>
    [[maybe_unused]] void error(fmt::format_string<Args...> fmt, Args && ... args) {
        impl::print(fg(impl::color::error()) | fmt::emphasis::bold, "[ERROR]", fmt, std::forward<Args>(args)...);
    }

    template<typename ... Args>
    [[maybe_unused]] void fatal(fmt::format_string<Args...> fmt, Args && ... args) {
        impl::print(fg(impl::color::fatal()) | fmt::emphasis::bold, "[FATAL]", fmt, std::forward<Args>(args)...);
    }

    template<typename ... Args>
    [[maybe_unused]] void print(fmt::format_string<Args...> fmt, Args && ... args) {
        impl::lockLoggerMutex();
        ON_SCOPE_EXIT { impl::unlockLoggerMutex(); };

        try {
            auto dest = impl::getDestination();
            fmt::print(dest, fmt, std::forward<Args>(args)...);
            std::fflush(dest);
        } catch (const std::exception&) {
            /* Ignore any exceptions, we can't do anything anyway */
        }
    }

    template<typename ... Args>
    [[maybe_unused]] void println(fmt::format_string<Args...> fmt, Args && ... args) {
        impl::lockLoggerMutex();
        ON_SCOPE_EXIT { impl::unlockLoggerMutex(); };

        try {
            auto dest = impl::getDestination();
            fmt::print(dest, fmt, std::forward<Args>(args)...);
            fmt::print("\n");
            std::fflush(dest);
        } catch (const std::exception&) {
            /* Ignore any exceptions, we can't do anything anyway */
        }
    }

}
