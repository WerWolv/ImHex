#pragma once

#if __has_include(<capstone/capstone.h>)
#include <capstone/capstone.h>
#else
#include <capstone.h>
#endif
#include <hex.hpp>

namespace hex {

    enum class Architecture : s32 {
        ARM,
        ARM64,
        MIPS,
        X86,
        PPC,
        SPARC,
        SYSZ,
        XCORE,
        M68K,
        TMS320C64X,
        M680X,
        EVM,

        MAX,
        MIN = ARM
    };

    class Disassembler {
    public:
        static constexpr cs_arch toCapstoneArchictecture(Architecture architecture) {
            return static_cast<cs_arch>(architecture);
        }

        static inline bool isSupported(Architecture architecture) {
            return cs_support(toCapstoneArchictecture(architecture));
        }

        constexpr static const char * const ArchitectureNames[] = { "ARM32", "ARM64", "MIPS", "x86", "PowerPC", "Sparc", "SystemZ", "XCore", "68K", "TMS320C64x", "680X", "Ethereum" };

        static inline s32 getArchitectureSupportedCount() {
            static s32 supportedCount = -1;

            if (supportedCount != -1) {
                return supportedCount;
            }

            for (supportedCount = static_cast<s32>(Architecture::MIN); supportedCount < static_cast<s32>(Architecture::MAX); supportedCount++) {
                if (!cs_support(supportedCount)) {
                    break;
                }
            }

            return supportedCount;
        }
    };
}
