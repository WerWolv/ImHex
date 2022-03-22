#pragma once

#include <hex/helpers/concepts.hpp>

namespace hex {

    [[noreturn]] void unreachable() {
        __builtin_unreachable();
    }

}