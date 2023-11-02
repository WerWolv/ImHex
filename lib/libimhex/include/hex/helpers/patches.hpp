#pragma once

#include <hex.hpp>

#include <map>
#include <vector>

#include <wolv/utils/expected.hpp>

namespace hex {

    using Patches = std::map<u64, u8>;

    enum class IPSError {
        AddressOutOfRange,
        PatchTooLarge,
        InvalidPatchHeader,
        InvalidPatchFormat,
        MissingEOF
    };

    wolv::util::Expected<std::vector<u8>, IPSError> generateIPSPatch(const Patches &patches);
    wolv::util::Expected<std::vector<u8>, IPSError> generateIPS32Patch(const Patches &patches);

    wolv::util::Expected<Patches, IPSError> loadIPSPatch(const std::vector<u8> &ipsPatch);
    wolv::util::Expected<Patches, IPSError> loadIPS32Patch(const std::vector<u8> &ipsPatch);
}