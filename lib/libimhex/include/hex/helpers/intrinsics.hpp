#pragma once

namespace hex {

    void unused(auto && ... x) {
        ((void)x, ...);
    }

}