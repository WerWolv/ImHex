#pragma once

#include <hex/helpers/types.hpp>

#include <vector>

namespace hex::impl {

    inline void appendEncodingLineStartAddress(std::vector<u64> &lineStartAddresses, size_t line, u64 nextLineStartAddress) {
        if (line + 1 == lineStartAddresses.size())
            lineStartAddresses.push_back(nextLineStartAddress);
    }

}
