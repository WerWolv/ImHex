#pragma once

#include <hex.hpp>

#include <array>

#include <wolv/utils/string.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>

#define CAPSTONE_AARCH64_COMPAT_HEADER
#define CAPSTONE_SYSTEMZ_COMPAT_HEADER
#include <capstone/capstone.h>

#if CS_API_MAJOR <= 4
    #error "Capstone 4 and older is not supported."
#endif

namespace hex::plugin::disasm {

    // Make sure these are in the same order as in capstone.h
    enum class BuiltinArchitecture : i32 {
        ARM             = CS_ARCH_ARM,
        ARM64           = CS_ARCH_ARM64,
        SYSTEMZ         = CS_ARCH_SYSZ,
        MIPS            = CS_ARCH_MIPS,
        X86             = CS_ARCH_X86,
        POWERPC         = CS_ARCH_PPC,
        SPARC           = CS_ARCH_SPARC,
        XCORE           = CS_ARCH_XCORE,
        M68K            = CS_ARCH_M68K,
        TMS320C64X      = CS_ARCH_TMS320C64X,
        M680X           = CS_ARCH_M680X,
        EVM             = CS_ARCH_EVM,
        MOS65XX         = CS_ARCH_MOS65XX,
        WASM            = CS_ARCH_WASM,
        BPF             = CS_ARCH_BPF,
        RISCV           = CS_ARCH_RISCV,
        SUPERH          = CS_ARCH_SH,
        TRICORE         = CS_ARCH_TRICORE,

        #if CS_API_MAJOR >= 6
            ALPHA       = CS_ARCH_ALPHA,
            HPPA        = CS_ARCH_HPPA,
            LOONGARCH   = CS_ARCH_LOONGARCH,
            XTENSA      = CS_ARCH_XTENSA,
            ARC         = CS_ARCH_ARC,
        #endif

        MAX,
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
            std::array<const char *, static_cast<u32>(BuiltinArchitecture::MAX)> names = { };

            using enum BuiltinArchitecture;

            names[u8(ARM)]              = "ARM";
            names[u8(ARM64)]            = "AArch64";
            names[u8(MIPS)]             = "MIPS";
            names[u8(X86)]              = "x86";
            names[u8(POWERPC)]          = "PowerPC";
            names[u8(SPARC)]            = "SPARC";
            names[u8(SYSTEMZ)]          = "z/Architecture";
            names[u8(XCORE)]            = "xCORE";
            names[u8(M68K)]             = "M68K";
            names[u8(TMS320C64X)]       = "TMS320C64x";
            names[u8(M680X)]            = "M680X";
            names[u8(EVM)]              = "Ethereum VM";
            names[u8(WASM)]             = "WebAssembly";
            names[u8(RISCV)]            = "RISC-V";
            names[u8(MOS65XX)]          = "MOS65XX";
            names[u8(BPF)]              = "BPF";
            names[u8(SUPERH)]           = "SuperH";
            names[u8(TRICORE)]          = "TriCore";

            #if CS_API_MAJOR >= 6
                names[u8(ALPHA)]        = "Alpha";
                names[u8(HPPA)]         = "HP/PA";
                names[u8(LOONGARCH)]    = "LoongArch";
                names[u8(XTENSA)]       = "Xtensa";
                names[u8(ARC)]          = "ARC";
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
            const auto vectorString = wolv::util::splitString(std::string(string), ";");

            std::string archName;
            std::string options;
            archName = wolv::util::trim(vectorString[0]);
            if (vectorString.size() != 1) {
                options = wolv::util::trim(vectorString[1]);
            } else {
                options = "";
            }

            u32 arch = {};
            u32 mode = {};

            if (archName.ends_with("be") || archName.ends_with("eb")) {
                mode |= CS_MODE_BIG_ENDIAN;
                archName.pop_back();
                archName.pop_back();
            } else if (archName.ends_with("le") || archName.ends_with("el")) {
                archName.pop_back();
                archName.pop_back();
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
        #if CS_API_MAJOR >= 6
            else if (equalsIgnoreCase(archName, "alpha"))
                arch = CS_ARCH_ALPHA;
            else if (equalsIgnoreCase(archName, "hppa"))
                arch = CS_ARCH_HPPA;
            else if (equalsIgnoreCase(archName, "loongarch"))
                arch = CS_ARCH_LOONGARCH;
            else if (equalsIgnoreCase(archName, "xtensa"))
                arch = CS_ARCH_XTENSA;
            else if (equalsIgnoreCase(archName, "arc"))
                arch = CS_ARCH_ARC;
        #endif
            else
                throw std::runtime_error("Invalid disassembler architecture");

            auto optionsVector = wolv::util::splitString(std::string(options), ",");
            for (std::string_view option : optionsVector) {
                option = wolv::util::trim(option);

                bool shouldAdd = true;
                if (option.starts_with("no-")) {
                    shouldAdd = false;
                    option.remove_prefix(3);
                }

                constexpr static std::array<std::pair<std::string_view, cs_mode>, 110> Options = {{
                    // Common
                    { "16bit",      CS_MODE_16                      },
                    { "32bit",      CS_MODE_32                      },
                    { "64bit",      CS_MODE_64                      },

                    // ARM
                    { "cortex-m",   CS_MODE_MCLASS                  },
                    { "armv8",      CS_MODE_V8                      },
                    { "thumb",      CS_MODE_THUMB                   },

                #if CS_API_MAJOR >= 6
                    // ARM64
                    { "apple",      CS_MODE_APPLE_PROPRIETARY       },
                #endif

                    // SPARC
                    { "sparcv9",    CS_MODE_V9                      },

                    // PowerPC
                    { "qpx",        CS_MODE_QPX                     },
                    { "spe",        CS_MODE_SPE                     },
                    { "ps",         CS_MODE_PS                      },
                    { "booke",      CS_MODE_BOOKE                   },
                #if CS_API_MAJOR >= 6
                    { "aixos",      CS_MODE_AIX_OS                  },
                    { "pwr7",       CS_MODE_PWR7                    },
                    { "pwr8",       CS_MODE_PWR8                    },
                    { "pwr9",       CS_MODE_PWR9                    },
                    { "pwr10",      CS_MODE_PWR10                   },
                    { "future",     CS_MODE_PPC_ISA_FUTURE          },
                    { "aixas",      CS_MODE_MODERN_AIX_AS           },
                    { "msync",      CS_MODE_MSYNC                   },
                #endif

                    // M68K
                    { "68000",      CS_MODE_M68K_000                },
                    { "68010",      CS_MODE_M68K_010                },
                    { "68020",      CS_MODE_M68K_020                },
                    { "68030",      CS_MODE_M68K_030                },
                    { "68040",      CS_MODE_M68K_040                },
                    { "68060",      CS_MODE_M68K_060                },

                    // MIPS
                    { "micromips",  CS_MODE_MICRO                   },
                    { "mips2",      CS_MODE_MIPS2                   },
                    { "mips3",      CS_MODE_MIPS3                   },
                    { "mips32r6",   CS_MODE_MIPS32R6                },
                #if CS_API_MAJOR >= 6
                    { "mips1",      CS_MODE_MIPS1                   },
                    { "mips4",      CS_MODE_MIPS4                   },
                    { "mips5",      CS_MODE_MIPS5                   },
                    { "mips32r2",   CS_MODE_MIPS32R2                },
                    { "mips32r3",   CS_MODE_MIPS32R3                },
                    { "mips32r5",   CS_MODE_MIPS32R5                },
                    { "mips64r2",   CS_MODE_MIPS64R2                },
                    { "mips64r3",   CS_MODE_MIPS64R3                },
                    { "mips64r5",   CS_MODE_MIPS64R5                },
                    { "mips64r6",   CS_MODE_MIPS64R6                },
                    { "octeon" ,    CS_MODE_OCTEON                  },
                    { "octeonp",    CS_MODE_OCTEONP                 },
                    { "nanomips",   CS_MODE_NANOMIPS                },
                    { "nms1",       CS_MODE_NMS1                    },
                    { "i7200",      CS_MODE_I7200                   },
                    { "nofloat",    CS_MODE_MIPS_NOFLOAT            },
                    { "ptr64",      CS_MODE_MIPS_PTR64              },
                    { "micro32r3",  CS_MODE_MICRO32R3               },
                    { "micro32r6",  CS_MODE_MICRO32R6               },
                #endif
                    { "6301",       CS_MODE_M680X_6301              },
                    { "6309",       CS_MODE_M680X_6309              },
                    { "6800",       CS_MODE_M680X_6800              },
                    { "6801",       CS_MODE_M680X_6801              },

                    // M680X
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

                    // BPF
                    { "bpfe",       CS_MODE_BPF_EXTENDED            },

                    // RISC-V
                    { "rv32g",      CS_MODE_RISCV32                 },
                    { "rv64g",      CS_MODE_RISCV64                 },
                    { "riscvc",     CS_MODE_RISCVC                  },

                    // MOS65XX
                    { "6502",       CS_MODE_MOS65XX_6502            },
                    { "65c02",      CS_MODE_MOS65XX_65C02           },
                    { "w65c02",     CS_MODE_MOS65XX_W65C02          },
                    { "65816",      CS_MODE_MOS65XX_65816           },
                    { "long-m",     CS_MODE_MOS65XX_65816_LONG_M    },
                    { "long-x",     CS_MODE_MOS65XX_65816_LONG_X    },

                    // SuperH
                    { "sh2",        CS_MODE_SH2                     },
                    { "sh2a",       CS_MODE_SH2A                    },
                    { "sh3",        CS_MODE_SH3                     },
                    { "sh4",        CS_MODE_SH4                     },
                    { "sh4a",       CS_MODE_SH4A                    },
                    { "shfpu",      CS_MODE_SHFPU                   },
                    { "shdsp",      CS_MODE_SHDSP                   },

                    // Tricore
                    { "tc1.1",      CS_MODE_TRICORE_110             },
                    { "tc1.2",      CS_MODE_TRICORE_120             },
                    { "tc1.3",      CS_MODE_TRICORE_130             },
                    { "tc1.3.1",    CS_MODE_TRICORE_131             },
                    { "tc1.6",      CS_MODE_TRICORE_160             },
                    { "tc1.6.1",    CS_MODE_TRICORE_161             },
                    { "tc1.6.2",    CS_MODE_TRICORE_162             },

                #if CS_API_MAJOR >= 6
                    // HP/PA
                    { "hppa1.1",   CS_MODE_HPPA_11                  },
                    { "hppa2.0",   CS_MODE_HPPA_20                  },
                    { "hppa2.0w",  CS_MODE_HPPA_20W                 },

                    // LoongArch
                    { "loongarch32", CS_MODE_LOONGARCH32            },
                    { "loongarch64", CS_MODE_LOONGARCH64            },

                    // SystemZ
                    { "arch8",     CS_MODE_SYSTEMZ_ARCH8            },
                    { "arch9",     CS_MODE_SYSTEMZ_ARCH9            },
                    { "arch10",    CS_MODE_SYSTEMZ_ARCH10           },
                    { "arch11",    CS_MODE_SYSTEMZ_ARCH11           },
                    { "arch12",    CS_MODE_SYSTEMZ_ARCH12           },
                    { "arch13",    CS_MODE_SYSTEMZ_ARCH13           },
                    { "arch14",    CS_MODE_SYSTEMZ_ARCH14           },
                    { "z10",       CS_MODE_SYSTEMZ_Z10              },
                    { "z196",      CS_MODE_SYSTEMZ_Z196             },
                    { "zec12",     CS_MODE_SYSTEMZ_ZEC12            },
                    { "z13",       CS_MODE_SYSTEMZ_Z13              },
                    { "z14",       CS_MODE_SYSTEMZ_Z14              },
                    { "z15",       CS_MODE_SYSTEMZ_Z15              },
                    { "z16",       CS_MODE_SYSTEMZ_Z16              },
                    { "generic",   CS_MODE_SYSTEMZ_GENERIC          },

                    // Xtensa
                    { "esp32",     CS_MODE_XTENSA_ESP32             },
                    { "esp32s2",   CS_MODE_XTENSA_ESP32S2           },
                    { "esp8266",   CS_MODE_XTENSA_ESP8266           },
                #endif
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
