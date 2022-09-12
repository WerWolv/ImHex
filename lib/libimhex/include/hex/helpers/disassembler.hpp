#pragma once

#include <hex.hpp>

#include <capstone/capstone.h>

namespace hex {

    enum class Architecture : i32
    {
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
        MOS65XX,
        WASM,
        BPF,
        RISCV,

        MAX,
        MIN = ARM
    };

    class Disassembler {
    public:
        constexpr static cs_arch toCapstoneArchitecture(Architecture architecture) {
            return static_cast<cs_arch>(architecture);
        }

        static inline bool isSupported(Architecture architecture) {
            return cs_support(toCapstoneArchitecture(architecture));
        }

        constexpr static const char *const ArchitectureNames[] = { "ARM32", "ARM64", "MIPS", "x86", "PowerPC", "Sparc", "SystemZ", "XCore", "68K", "TMS320C64x", "680X", "Ethereum", "MOS65XX", "WebAssembly", "Berkeley Packet Filter", "RISC-V" };

        static inline i32 getArchitectureSupportedCount() {
            static i32 supportedCount = -1;

            if (supportedCount != -1) {
                return supportedCount;
            }

            for (supportedCount = static_cast<i32>(Architecture::MIN); supportedCount < static_cast<i32>(Architecture::MAX); supportedCount++) {
                if (!cs_support(supportedCount)) {
                    break;
                }
            }

            return supportedCount;
        }
    };
}
