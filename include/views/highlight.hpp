#pragma once

#include <hex.hpp>
#include <string>

namespace hex {

    struct Highlight {
        Highlight(u64 offset, size_t size, u32 color, std::string name)
        : offset(offset), size(size), color(color), name(name) {

        }

        u64 offset;
        size_t size;
        u32 color;
        std::string name;
    };

}