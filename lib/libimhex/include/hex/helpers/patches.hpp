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

    enum class PatchKind {
        IPS,
        IPS32
    };

    class Patches {
    public:
        Patches() = default;
        Patches(std::map<u64, u8> &&patches) : m_patches(std::move(patches)) {}

        static wolv::util::Expected<Patches, IPSError> fromProvider(hex::prv::Provider *provider);
        static wolv::util::Expected<Patches, IPSError> fromIPSPatch(const std::vector<u8> &ipsPatch);
        static wolv::util::Expected<Patches, IPSError> fromIPS32Patch(const std::vector<u8> &ipsPatch);

        wolv::util::Expected<std::vector<u8>, IPSError> toIPSPatch() const;
        wolv::util::Expected<std::vector<u8>, IPSError> toIPS32Patch() const;

        const auto& get() const { return m_patches; }
        auto& get() { return m_patches; }

    private:
        std::map<u64, u8> m_patches;
    };
}