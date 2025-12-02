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