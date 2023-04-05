#pragma once

#include <hex.hpp>

#include <capstone/capstone.h>

namespace hex {

    enum class Architecture : i32
    {
        ARM         = CS_ARCH_ARM,
        ARM64       = CS_ARCH_ARM64,
        MIPS        = CS_ARCH_MIPS,
        X86         = CS_ARCH_X86,
        PPC         = CS_ARCH_PPC,
        SPARC       = CS_ARCH_SPARC,
        SYSZ        = CS_ARCH_SYSZ,
        XCORE       = CS_ARCH_XCORE,
        M68K        = CS_ARCH_M68K,
        TMS320C64X  = CS_ARCH_TMS320C64X,
        M680X       = CS_ARCH_M680X,
        EVM         = CS_ARCH_EVM,
        WASM        = CS_ARCH_WASM,

        #if CS_API_MAJOR >= 5
            RISCV   = CS_ARCH_RISCV,
            MOS65XX = CS_ARCH_MOS65XX,
            BPF     = CS_ARCH_BPF,
        #endif

        MAX         = CS_ARCH_MAX,
        MIN         = ARM
    };

    class Disassembler {
    public:
        constexpr static cs_arch toCapstoneArchitecture(Architecture architecture) {
            return static_cast<cs_arch>(architecture);
        }

        static inline bool isSupported(Architecture architecture) {
            return cs_support(toCapstoneArchitecture(architecture));
        }

        constexpr static auto ArchitectureNames = [](){
            std::array<const char *, static_cast<u32>(Architecture::MAX)> names = { };

            names[CS_ARCH_ARM]          = "ARM";
            names[CS_ARCH_ARM64]        = "AArch64";
            names[CS_ARCH_MIPS]         = "MIPS";
            names[CS_ARCH_X86]          = "Intel x86";
            names[CS_ARCH_PPC]          = "PowerPC";
            names[CS_ARCH_SPARC]        = "SPARC";
            names[CS_ARCH_SYSZ]         = "SystemZ";
            names[CS_ARCH_XCORE]        = "XCore";
            names[CS_ARCH_M68K]         = "Motorola 68K";
            names[CS_ARCH_TMS320C64X]   = "TMS320C64x";
            names[CS_ARCH_M680X]        = "M680X";
            names[CS_ARCH_EVM]          = "Ethereum Virtual Machine";
            names[CS_ARCH_WASM]         = "WebAssembly";

            #if CS_API_MAJOR >= 5
                names[CS_ARCH_RISCV]    = "RISC-V";
                names[CS_ARCH_MOS65XX]  = "MOS Technology 65xx";
                names[CS_ARCH_BPF]      = "Berkeley Packet Filter";
            #endif

            return names;
        }();

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
