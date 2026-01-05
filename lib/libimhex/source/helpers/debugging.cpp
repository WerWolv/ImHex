#include <hex/helpers/debugging.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/trace/stacktrace.hpp>

namespace hex::dbg {

    namespace impl {

        bool &getDebugWindowState() {
            static bool state = false;

            return state;
        }

    }

    static bool s_debugMode = false;
    bool debugModeEnabled() {
        return s_debugMode;
    }

    void setDebugModeEnabled(bool enabled) {
        s_debugMode = enabled;
    }

    [[noreturn]] void assertionHandler(const char* file, int line, const char *function, const char* exprString) {
        log::error("Assertion failed: {} at {}:{} => {}", exprString, file, line, function);

        const auto stackTrace = trace::getStackTrace();
        dbg::printStackTrace(stackTrace);

        std::abort();
    }

    void printStackTrace(const trace::StackTraceResult &stackTrace) {
        log::fatal("Printing stacktrace using implementation '{}'", stackTrace.implementationName);
        for (const auto &stackFrame : stackTrace.stackFrames) {
            if (stackFrame.line == 0)
                log::fatal("  ({}) | {}", stackFrame.file, stackFrame.function);
            else
                log::fatal("  ({}:{}) | {}",  stackFrame.file, stackFrame.line, stackFrame.function);
        }
    }

}