#include <hex/helpers/logger.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/fmt.hpp>

#include <wolv/io/file.hpp>

#if defined(OS_WINDOWS)
    #include <Windows.h>
#endif

namespace hex::log::impl {

    static wolv::io::File s_loggerFile;
    std::mutex g_loggerMutex;

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

        for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Logs, true)) {
            wolv::io::fs::createDirectories(path);
            s_loggerFile = wolv::io::File(path / hex::format("{0:%Y%m%d_%H%M%S}.log", fmt::localtime(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()))), wolv::io::File::Mode::Create);
            s_loggerFile.disableBuffering();

            if (s_loggerFile.isValid()) break;
        }
    }

    void enableColorPrinting() {
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

    void assertionHandler(bool expr, const char* expr_str, const char* file, int line) {
        if (!expr) {
            log::error("Assertion failed: {} at {}:{}", expr_str, file, line);

            #if defined (DEBUG)
                std::abort();
            #endif
        }
    }

}