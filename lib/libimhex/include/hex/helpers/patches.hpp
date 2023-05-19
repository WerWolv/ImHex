#pragma once

#include <hex.hpp>

#include <map>
#include <vector>


#if defined(LIBIMHEX_EXPECTED_NS_STD) || (!defined(LIBIMHEX_EXPECTED_NS_TL) && !defined(LIBIMHEX_EXPECTED_NS_RD) && !defined(LIBIMHEX_EXPECTED_NS_NONSTD))
    #include <expected>
    #ifndef LIBIMHEX_EXPECTED_NS_STD
        #define LIBIMHEX_EXPECTED_NS_STD
    #endif
#else
    #ifdef LIBIMHEX_EXPECTED_NS_TL
        #include <tl/expected.hpp>
    #endif
    #ifdef LIBIMHEX_EXPECTED_NS_RD
        #include <rd/expected.hpp>
    #endif
    #ifdef LIBIMHEX_EXPECTED_NS_NONSTD
        #define nsel_CONFIG_SELECT_EXPECTED nsel_EXPECTED_NONSTD
        #include <nonstd/expected.hpp>
    #endif
#endif


namespace hex {
    #ifdef LIBIMHEX_EXPECTED_NS_STD
        template <typename T, typename E>
        using expected = std::expected<T, E>;

        template <typename E>
        using unexpeced = std::unexpected<E>;
    #else
        #ifdef LIBIMHEX_EXPECTED_NS_TL
            template <typename T, typename E>
            using expected = tl::expected<T, E>;

            template <typename E>
            using unexpected = tl::unexpected<E>;
        #endif
        #ifdef LIBIMHEX_EXPECTED_NS_RD
            template <typename T, typename E>
            using expected = rd::expected<T, E>;

            template <typename E>
            using unexpected = rd::unexpected<E>;
        #endif
        #ifdef LIBIMHEX_EXPECTED_NS_NONSTD
            template <typename T, typename E>
            using expected = nonstd::expected<T, E>;

            template <typename E>
            using unexpected = nonstd::unexpected_type<E>;
        #endif
    #endif

    using Patches = std::map<u64, u8>;

    enum class IPSError {
        AddressOutOfRange,
        PatchTooLarge,
        InvalidPatchHeader,
        InvalidPatchFormat,
        MissingEOF
    };

    expected<std::vector<u8>, IPSError> generateIPSPatch(const Patches &patches);
    expected<std::vector<u8>, IPSError> generateIPS32Patch(const Patches &patches);

    expected<Patches, IPSError> loadIPSPatch(const std::vector<u8> &ipsPatch);
    expected<Patches, IPSError> loadIPS32Patch(const std::vector<u8> &ipsPatch);
}
