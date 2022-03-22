#pragma once

#include <hex/helpers/concepts.hpp>

namespace hex {

    [[noreturn]] inline void unreachable() {
        __builtin_unreachable();
    }

}