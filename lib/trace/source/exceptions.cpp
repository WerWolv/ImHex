#include <hex/trace/exceptions.hpp>

namespace hex::trace {

    static std::optional<StackTraceResult> s_lastExceptionStackTrace;
    static thread_local bool s_threadExceptionCaptureEnabled = false;

    std::optional<StackTraceResult> getLastExceptionStackTrace() {
        if (!s_lastExceptionStackTrace.has_value())
            return std::nullopt;

        auto result = s_lastExceptionStackTrace.value();
        s_lastExceptionStackTrace.reset();

        return result;
    }

    void enableExceptionCaptureForCurrentThread() {
        s_threadExceptionCaptureEnabled = true;
    }

}

#if defined(HEX_WRAP_CXA_THROW)

    extern "C" {

        [[noreturn]] void __real___cxa_throw(void* thrownException, void* type, void (*destructor)(void*));
        [[noreturn]] void __wrap___cxa_throw(void* thrownException, void* type, void (*destructor)(void*)) {
            if (hex::trace::s_threadExceptionCaptureEnabled)
                hex::trace::s_lastExceptionStackTrace = hex::trace::getStackTrace();

            __real___cxa_throw(thrownException, type, destructor);
        }

    }

#endif

#if defined(HEX_WRAP_GLIBCXX_ASSERT_FAIL)

extern "C" {

    [[noreturn]] void __wrap__ZSt21__glibcxx_assert_failPKciS0_S0_(const char* file, int line, const char* function, const char* condition) {
        if (file != nullptr && function != nullptr && condition != nullptr) {
            fprintf(stderr, "Assertion failed (glibc++): (%s), function %s, file %s, line %d.\n", condition, function, file, line);
        } else if (function != nullptr) {
            fprintf(stderr, "%s: Undefined behavior detected (glibc++).\n", function);
        }

        auto stackTrace = hex::trace::getStackTrace();
        for (const auto &entry : stackTrace.stackFrames) {
            fprintf(stderr, "  %s at %s:%d\n", entry.function.c_str(), entry.file.c_str(), entry.line);
        }

        std::terminate();
    }

}

#endif