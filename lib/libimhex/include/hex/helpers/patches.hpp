#pragma once

#include <hex.hpp>

#include <map>
#include <vector>
#include <expected>

namespace hex {

    using Patches = std::map<u64, u8>;

    enum class IPSError {
        AddressOutOfRange,
        PatchTooLarge,
        InvalidPatchHeader,
        InvalidPatchFormat,
        MissingEOF
    };

    std::expected<std::vector<u8>, IPSError> generateIPSPatch(const Patches &patches);
    std::expected<std::vector<u8>, IPSError> generateIPS32Patch(const Patches &patches);

    std::expected<Patches, IPSError> loadIPSPatch(const std::vector<u8> &ipsPatch);
    std::expected<Patches, IPSError> loadIPS32Patch(const std::vector<u8> &ipsPatch);
}