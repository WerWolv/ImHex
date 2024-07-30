#include <hex/helpers/logger.hpp>

#include <hex/api/task_manager.hpp>
#include <hex/api/event_manager.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/default_paths.hpp>

#include <wolv/io/file.hpp>

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
                s_loggerFile = wolv::io::File(path / hex::format("{0:%Y%m%d_%H%M%S}.log", fmt::localtime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()))), wolv::io::File::Mode::Create);
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


        std::vector<LogEntry>& getLogEntries() {
            static std::vector<LogEntry> logEntries;
            return logEntries;
        }

        void addLogEntry(std::string_view project, std::string_view level, std::string_view message) {
            getLogEntries().emplace_back(project.data(), level.data(), message.data());
        }


        void printPrefix(FILE *dest, const fmt::text_style &ts, const std::string &level, const char *projectName) {
            const auto now = fmt::localtime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

            fmt::print(dest, "[{0:%H:%M:%S}] ", now);

            if (s_colorOutputEnabled)
                fmt::print(dest, ts, "{0} ", level);
            else
                fmt::print(dest, "{0} ", level);

            std::string projectThreadTag = projectName;
            if (auto threadName = TaskManager::getCurrentThreadName(); !threadName.empty())
                projectThreadTag += fmt::format(" | {0}", threadName);

            constexpr static auto MaxTagLength = 25;
            if (projectThreadTag.length() > MaxTagLength)
                projectThreadTag.resize(MaxTagLength);

            fmt::print(dest, "[{0}] ", projectThreadTag);

            const auto projectNameLength = projectThreadTag.length();
            fmt::print(dest, "{0}", std::string(projectNameLength > MaxTagLength ? 0 : MaxTagLength - projectNameLength, ' '));
        }

        void assertionHandler(const char* exprString, const char* file, int line) {
            log::error("Assertion failed: {} at {}:{}", exprString, file, line);

            #if defined (DEBUG)
                std::abort();
            #endif
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