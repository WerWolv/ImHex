#pragma once

#include <hex.hpp>

#include <array>

#include <capstone/capstone.h>
#include <wolv/utils/string.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::plugin::disasm {

    enum class BuiltinArchitecture : i32
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

        #if CS_API_MAJOR >= 5
            WASM    = CS_ARCH_WASM,
            RISCV   = CS_ARCH_RISCV,
            MOS65XX = CS_ARCH_MOS65XX,
            BPF     = CS_ARCH_BPF,
            SUPERH  = CS_ARCH_SH,
            TRICORE = CS_ARCH_TRICORE,
            MAX   = TRICORE,
        # else
            MAX   = EVM,
        #endif

        MIN         = ARM
    };

    class CapstoneDisassembler {
    public:
        constexpr static cs_arch toCapstoneArchitecture(BuiltinArchitecture architecture) {
            return static_cast<cs_arch>(architecture);
        }

        static bool isSupported(BuiltinArchitecture architecture) {
            return cs_support(toCapstoneArchitecture(architecture));
        }

        constexpr static auto ArchitectureNames = []{
            std::array<const char *, static_cast<u32>(BuiltinArchitecture::MAX) + 1> names = { };

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

            #if CS_API_MAJOR >= 5
                names[CS_ARCH_WASM]     = "WebAssembly";
                names[CS_ARCH_RISCV]    = "RISC-V";
                names[CS_ARCH_MOS65XX]  = "MOS Technology 65xx";
                names[CS_ARCH_BPF]      = "Berkeley Packet Filter";
                names[CS_ARCH_SH]       = "SuperH";
                names[CS_ARCH_TRICORE]  = "Tricore";
            #endif

            return names;
        }();

        static i32 getArchitectureSupportedCount() {
            static i32 supportedCount = -1;

            if (supportedCount != -1) {
                return supportedCount;
            }

            for (supportedCount = static_cast<i32>(BuiltinArchitecture::MIN); supportedCount < static_cast<i32>(BuiltinArchitecture::MAX) + 1; supportedCount++) {
                if (!cs_support(supportedCount)) {
                    break;
                }
            }

            return supportedCount;
        }

        // string has to be in the form of `arch;option1,option2,option3,no-option4`
        // Not all results might make sense for capstone
        static std::pair<cs_arch, cs_mode> stringToSettings(std::string_view string) {
            const auto archSeparator = string.find_first_of(';');

            std::string_view archName;
            std::string_view options;
            if (archSeparator == std::string_view::npos) {
                archName = wolv::util::trim(string);
                options = "";
            } else {
                archName = wolv::util::trim(string.substr(0, archSeparator - 1));
                options = wolv::util::trim(string.substr(archSeparator + 1));
            }

            u32 arch = {};
            u32 mode = {};

            if (archName.ends_with("be") || archName.ends_with("eb")) {
                mode |= CS_MODE_BIG_ENDIAN;
                archName.remove_suffix(2);
            } else if (archName.ends_with("le") || archName.ends_with("el")) {
                mode |= CS_MODE_LITTLE_ENDIAN;
                archName.remove_suffix(2);
            }

            if (equalsIgnoreCase(archName, "arm")) {
                arch = CS_ARCH_ARM;
                mode |= CS_MODE_ARM;
            }
            else if (equalsIgnoreCase(archName, "thumb")) {
                arch = CS_ARCH_ARM;
                mode |= CS_MODE_THUMB;
            }
            else if (equalsIgnoreCase(archName, "aarch64") || equalsIgnoreCase(archName, "arm64"))
                arch = CS_ARCH_ARM64;
            else if (equalsIgnoreCase(archName, "mips"))
                arch = CS_ARCH_MIPS;
            else if (equalsIgnoreCase(archName, "x86"))
                arch = CS_ARCH_X86;
            else if (equalsIgnoreCase(archName, "x86_64") || equalsIgnoreCase(archName, "x64")) {
                arch = CS_ARCH_X86;
                mode = CS_MODE_64;
            }
            else if (equalsIgnoreCase(archName, "ppc") || equalsIgnoreCase(archName, "powerpc"))
                arch = CS_ARCH_PPC;
            else if (equalsIgnoreCase(archName, "sparc"))
                arch = CS_ARCH_SPARC;
            else if (equalsIgnoreCase(archName, "sysz"))
                arch = CS_ARCH_SYSZ;
            else if (equalsIgnoreCase(archName, "xcore"))
                arch = CS_ARCH_XCORE;
            else if (equalsIgnoreCase(archName, "m68k"))
                arch = CS_ARCH_M68K;
            else if (equalsIgnoreCase(archName, "m680x"))
                arch = CS_ARCH_M680X;
            else if (equalsIgnoreCase(archName, "tms320c64x"))
                arch = CS_ARCH_TMS320C64X;
            else if (equalsIgnoreCase(archName, "evm"))
                arch = CS_ARCH_EVM;
        #if CS_API_MAJOR >= 5
            else if (equalsIgnoreCase(archName, "wasm"))
                arch = CS_ARCH_WASM;
            else if (equalsIgnoreCase(archName, "riscv"))
                arch = CS_ARCH_RISCV;
            else if (equalsIgnoreCase(archName, "mos65xx"))
                arch = CS_ARCH_MOS65XX;
            else if (equalsIgnoreCase(archName, "bpf"))
                arch = CS_ARCH_BPF;
            else if (equalsIgnoreCase(archName, "sh") || equalsIgnoreCase(archName, "superh"))
                arch = CS_ARCH_SH;
            else if (equalsIgnoreCase(archName, "tricore"))
                arch = CS_ARCH_TRICORE;
        #endif
            else
                throw std::runtime_error("Invalid disassembler architecture");

            while (!options.empty()) {
                std::string_view option;
                auto separatorPos = options.find_first_of(',');
                if (separatorPos == std::string_view::npos)
                    option = options;
                else
                    option = options.substr(0, separatorPos - 1);

                options.remove_prefix(option.size() + 1);
                option = wolv::util::trim(option);

                bool shouldAdd = true;
                if (option.starts_with("no-")) {
                    shouldAdd = false;
                    option.remove_prefix(3);
                }

                constexpr static std::array<std::pair<std::string_view, cs_mode>, 53> Options = {{
                    { "16bit",      CS_MODE_16                      },
                    { "32bit",      CS_MODE_32                      },
                    { "64bit",      CS_MODE_64                      },
                    { "cortex-m",   CS_MODE_MCLASS                  },
                    { "armv8",      CS_MODE_V8                      },
                    { "micromips",  CS_MODE_MICRO                   },
                    { "mips2",      CS_MODE_MIPS2                   },
                    { "mips3",      CS_MODE_MIPS3                   },
                    { "mips32r6",   CS_MODE_MIPS32R6                },
                    { "sparcv9",    CS_MODE_V9                      },
                    { "qpx",        CS_MODE_QPX                     },
                    { "spe",        CS_MODE_SPE                     },
                    { "ps",         CS_MODE_PS                      },
                    { "68000",      CS_MODE_M68K_000                },
                    { "68010",      CS_MODE_M68K_010                },
                    { "68020",      CS_MODE_M68K_020                },
                    { "68030",      CS_MODE_M68K_030                },
                    { "68040",      CS_MODE_M68K_040                },
                    { "68060",      CS_MODE_M68K_060                },
                    { "6301",       CS_MODE_M680X_6301              },
                    { "6309",       CS_MODE_M680X_6309              },
                    { "6800",       CS_MODE_M680X_6800              },
                    { "6801",       CS_MODE_M680X_6801              },
                    { "6805",       CS_MODE_M680X_6805              },
                    { "6808",       CS_MODE_M680X_6808              },
                    { "6809",       CS_MODE_M680X_6809              },
                    { "6811",       CS_MODE_M680X_6811              },
                    { "cpu12",      CS_MODE_M680X_CPU12             },
                    { "hcs08",      CS_MODE_M680X_HCS08             },
                    { "bpfe",       CS_MODE_BPF_EXTENDED            },
                    { "rv32g",      CS_MODE_RISCV32                 },
                    { "rv64g",      CS_MODE_RISCV64                 },
                    { "riscvc",     CS_MODE_RISCVC                  },
                    { "6502",       CS_MODE_MOS65XX_6502            },
                    { "65c02",      CS_MODE_MOS65XX_65C02           },
                    { "w65c02",     CS_MODE_MOS65XX_W65C02          },
                    { "65816",      CS_MODE_MOS65XX_65816           },
                    { "long-m",     CS_MODE_MOS65XX_65816_LONG_M    },
                    { "long-x",     CS_MODE_MOS65XX_65816_LONG_X    },
                    { "sh2",        CS_MODE_SH2                     },
                    { "sh2a",       CS_MODE_SH2A                    },
                    { "sh3",        CS_MODE_SH3                     },
                    { "sh4",        CS_MODE_SH4                     },
                    { "sh4a",       CS_MODE_SH4A                    },
                    { "shfpu",      CS_MODE_SHFPU                   },
                    { "shdsp",      CS_MODE_SHDSP                   },
                    { "1.1",        CS_MODE_TRICORE_110             },
                    { "1.2",        CS_MODE_TRICORE_120             },
                    { "1.3",        CS_MODE_TRICORE_130             },
                    { "1.3.1",      CS_MODE_TRICORE_131             },
                    { "1.6",        CS_MODE_TRICORE_160             },
                    { "1.6.1",      CS_MODE_TRICORE_161             },
                    { "1.6.2",      CS_MODE_TRICORE_162             },
                }};

                bool optionFound = false;
                for (const auto &[optionName, optionValue] : Options) {
                    if (equalsIgnoreCase(option, optionName)) {
                        if (shouldAdd) mode |=  optionValue;
                        else           mode &= ~optionValue;

                        optionFound = true;
                        break;
                    }
                }

                if (!optionFound) {
                    throw std::runtime_error(fmt::format("Unknown disassembler option '{}'", option));
                }
            }

            return { cs_arch(arch), cs_mode(mode) };
        }
    };
}
