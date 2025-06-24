#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hex::trace {

    struct StackFrame {
        std::string file;
        std::string function;
        std::uint32_t line;
    };

    using StackTrace = std::vector<StackFrame>;

    void initialize();

    struct StackTraceResult {
        std::vector<StackFrame> stackFrames;
        std::string implementationName;
    };

    StackTraceResult getStackTrace(); 

}