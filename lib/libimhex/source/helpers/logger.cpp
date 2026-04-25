#include <hex/helpers/logger.hpp>

#include <hex/api/task_manager.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/default_paths.hpp>
#include <hex/helpers/auto_reset.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/string.hpp>

#include <mutex>
#include <chrono>
#include <fmt/chrono.h>

#if defined(OS_WINDOWS)
    #include <Windows.h>
#endif

namespace hex::log {

    namespace {

        wolv::io::File s_loggerFile;
        bool s_colorOutputEnabled = false;
        std::recursive_mutex s_loggerMutex;
        bool s_loggingSuspended = false;
        bool s_debugLoggingEnabled = false;

    }

    void suspendLogging() {
        s_loggingSuspended = true;
    }

    void resumeLogging() {
        s_loggingSuspended = false;
    }

    void enableDebugLogging() {
        s_debugLoggingEnabled = true;
    }

    namespace impl {

        void lockLoggerMutex() {
            s_loggerMutex.lock();
        }

        void unlockLoggerMutex() {
            s_loggerMutex.unlock();
        }


        bool isLoggingSuspended() {
            return s_loggingSuspended;
        }

        bool isDebugLoggingEnabled() {
            #if defined(DEBUG)
                return true;
            #else
                return s_debugLoggingEnabled;
            #endif
        }

        FILE *getDestination() {
            if (s_loggerFile.isValid())
                return s_loggerFile.getHandle();
            else
                return stdout;
        }

        wolv::io::File& getFile() {
            return s_loggerFile;
        }

        bool isRedirected() {
            return s_loggerFile.isValid();
        }

        void redirectToFile() {
            if (s_loggerFile.isValid()) return;

            for (const auto &path : paths::Logs.all()) {
                wolv::io::fs::createDirectories(path);
                time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                s_loggerFile = wolv::io::File(path / fmt::format("{0:%Y%m%d_%H%M%S}.log", *std::localtime(&time)), wolv::io::File::Mode::Create);
                s_loggerFile.disableBuffering();

                if (s_loggerFile.isValid()) {
                    s_colorOutputEnabled = false;
                    break;
                }
            }
        }

        void enableColorPrinting() {
            s_colorOutputEnabled = true;

            #if defined(OS_WINDOWS)
                auto hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
                if (hConsole != INVALID_HANDLE_VALUE) {
                    DWORD mode = 0;
                    if (::GetConsoleMode(hConsole, &mode) == TRUE) {
                        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
                        ::SetConsoleMode(hConsole, mode);
                    }
                }
            #endif
        }

        static AutoReset<std::vector<LogEntry>> s_logEntries;
        const std::vector<LogEntry>& getLogEntries() {
            return s_logEntries;
        }

        void addLogEntry(std::string_view project, std::string_view level, std::string message) {
            s_logEntries->emplace_back(
                project,
                level,
                std::move(message)
            );
        }


        void printPrefix(FILE *dest, fmt::text_style ts, std::string_view level, std::string_view projectName) {
            const auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            const auto now = *std::localtime(&time);

            auto threadName = TaskManager::getCurrentThreadName(); // may contain multibyte UTF-8 characters
            if (threadName.empty()) [[unlikely]] {
                threadName = "???";
            }

            // Prefix format:
            // |<-time->| |level|  |<------ tag ----->|
            // [04:24:08] [DEBUG] [builtin | Init Tasks]
            // |<---------- ASCII --------->||<-UTF8->|

            constexpr static auto MaxTagLength = 25;

            auto tag = fmt::format("{} | {}", projectName, threadName);
            auto fixedLengthTag = fmt::format("{:<{}.{}}", tag, MaxTagLength, MaxTagLength);
            // This step is needed to avoid chopping off closing bracket
            if (fixedLengthTag.length() > tag.length())
                tag = fmt::format("[{}]{}  ", tag, fixedLengthTag.substr(tag.length()));
            else
                tag = fmt::format("[{}]  ", fixedLengthTag);
            fmt::print(dest, "[{0:%H:%M:%S}] {1} {2}", now, s_colorOutputEnabled ? fmt::format(ts, "{}", level) : level, tag);
        }   

        namespace color {

            fmt::color debug() { return fmt::color::medium_sea_green; }
            fmt::color info()  { return fmt::color::steel_blue; }
            fmt::color warn()  { return fmt::color::orange; }
            fmt::color error() { return fmt::color::indian_red; }
            fmt::color fatal() { return fmt::color::medium_purple; }

        }

    }

}