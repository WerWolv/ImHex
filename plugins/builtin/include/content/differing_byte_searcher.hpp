#pragma once

#include <functional>
#include <hex/providers/provider.hpp>
#include <wolv/types.hpp>

namespace hex::plugin::builtin {

    void findNextDifferingByte(
        const std::function< u64(prv::Provider*) >& lastValidAddressProvider,
        const std::function< bool(u64, u64) >& addressComparator,
        const std::function< void(u64*) >& addressStepper,
        bool *didFindNextValue,
        bool *didReachEndAddress,
        u64* foundAddress
    );

    bool canSearchForDifferingByte();
}