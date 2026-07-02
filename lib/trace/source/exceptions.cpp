#include <hex/trace/exceptions.hpp>

namespace hex::trace {

    static thread_local std::optional<StackTraceResult> s_lastExceptionStackTrace;
    static thread_local bool s_threadExceptionCaptureEnabled = false;
    static AssertionHandler s_assertionHandler = nullptr;

    std::optional<StackTraceResult> getLastExceptionStackTrace() {
        if (!s_lastExceptionStackTrace.has_value())
            return std::nullopt;

        auto result = s_lastExceptionStackTrace.value();
        s_lastExceptionStackTrace.reset();

        return result;
    }

    void setAssertionHandler(AssertionHandler handler) {
        s_assertionHandler = handler;
    }

    void enableExceptionCaptureForCurrentThread() {
        s_threadExceptionCaptureEnabled = true;
    }

    void disableExceptionCaptureForCurrentThread() {
        s_threadExceptionCaptureEnabled = false;
    }

}

#if defined(HEX_WRAP_GLIBCXX_ASSERT_FAIL)

extern "C" {

    [[noreturn]] void __real__ZSt21__glibcxx_assert_failPKciS0_S0_(const char* file, int line, const char* function, const char* condition);
    [[noreturn]] void __wrap__ZSt21__glibcxx_assert_failPKciS0_S0_(const char* file, int line, const char* function, const char* condition) {
        if (hex::trace::s_assertionHandler != nullptr) {
            hex::trace::s_assertionHandler(file, line, function, condition);
        } else {
            __real__ZSt21__glibcxx_assert_failPKciS0_S0_(file, line, function, condition);
        }

        std::abort();
    }

}

#endif