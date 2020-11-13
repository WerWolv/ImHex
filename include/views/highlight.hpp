#pragma once

#include <hex.hpp>
#include <string>

namespace hex {

    struct VariableType {
        size_t size;
        enum class Kind { Unsigned, Signed, FloatingPoint } kind;
    };

    struct Highlight {
        Highlight(u64 offset, VariableType type, u32 color, std::string name)
        : offset(offset), type(type), color(color), name(name) {

        }

        u64 offset;
        VariableType type;
        u32 color;
        std::string name;
    };

}