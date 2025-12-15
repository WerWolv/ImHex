#pragma once

#include <hex/trace/stacktrace.hpp>

#include <optional>

namespace hex::trace {

    using AssertionHandler = void(*)(const char* file, int line, const char *function, const char* exprString);

    std::optional<StackTraceResult> getLastExceptionStackTrace();
    void setAssertionHandler(AssertionHandler handler);

    void enableExceptionCaptureForCurrentThread();
    void disableExceptionCaptureForCurrentThread();

}