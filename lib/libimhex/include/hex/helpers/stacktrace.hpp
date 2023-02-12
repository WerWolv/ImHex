#pragma once

#include <hex.hpp>

#include <string>
#include <vector>

namespace hex::stacktrace {

    struct StackFrame {
        std::string file;
        std::string function;
        u32 line;
    };

    void initialize();

    std::vector<StackFrame> getStackTrace(); 

}