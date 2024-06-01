#include <hex/trace/exceptions.hpp>

namespace hex::trace {

    static std::optional<StackTrace> s_lastExceptionStackTrace;
    std::optional<StackTrace> getLastExceptionStackTrace() {
        if (!s_lastExceptionStackTrace.has_value())
            return std::nullopt;

        auto result = s_lastExceptionStackTrace.value();
        s_lastExceptionStackTrace.reset();

        return result;
    }

}

extern "C" {

    [[noreturn]] void __real___cxa_throw(void* thrownException, void* type, void (*destructor)(void*));
    [[noreturn]] void __wrap___cxa_throw(void* thrownException, void* type, void (*destructor)(void*)) {
        hex::trace::s_lastExceptionStackTrace = hex::trace::getStackTrace();
        __real___cxa_throw(thrownException, type, destructor);
    }

}

