#pragma once

#include <hex.hpp>

#include <map>
#include <vector>

#include <wolv/utils/expected.hpp>

namespace hex {

    namespace prv {
        class Provider;
    }

    enum class IPSError {
        AddressOutOfRange,
        PatchTooLarge,
        InvalidPatchHeader,
        InvalidPatchFormat,
        MissingEOF
    };

    class Patches : public std::map<u64, u8> {
    public:
        using std::map<u64, u8>::map;

        static wolv::util::Expected<Patches, IPSError> fromProvider(hex::prv::Provider *provider);
        static wolv::util::Expected<Patches, IPSError> fromIPSPatch(const std::vector<u8> &ipsPatch);
        static wolv::util::Expected<Patches, IPSError> fromIPS32Patch(const std::vector<u8> &ipsPatch);

        wolv::util::Expected<std::vector<u8>, IPSError> toIPSPatch() const;
        wolv::util::Expected<std::vector<u8>, IPSError> toIPS32Patch() const;
    };
}