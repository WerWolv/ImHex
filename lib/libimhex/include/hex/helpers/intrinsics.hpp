#pragma once

namespace hex {

    [[noreturn]] inline void unreachable() {
        __builtin_unreachable();
    }

    inline void unused(auto && ... x) {
        ((void)x, ...);
    }

}