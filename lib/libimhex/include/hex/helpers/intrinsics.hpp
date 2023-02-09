#pragma once

namespace hex {

    inline void unused(auto && ... x) {
        ((void)x, ...);
    }

}