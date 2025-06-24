#pragma once

#include <hex/trace/stacktrace.hpp>

#include <optional>

namespace hex::trace {

    std::optional<StackTraceResult> getLastExceptionStackTrace();

}