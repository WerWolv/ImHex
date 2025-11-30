#include <hex/api/content_registry/disassemblers.hpp>

#include <content/helpers/capstone.hpp>
#include <hex/helpers/fmt.hpp>

#include <imgui.h>

namespace hex::plugin::disasm {

    class CapstoneArchitecture : public ContentRegistry::Disassemblers::Architecture {
    public:
        explicit CapstoneArchitecture(BuiltinArchitecture architecture, cs_mode mode = cs_mode(0))
            : Architecture(CapstoneDisassembler::ArchitectureNames[u32(architecture)]),
              m_architecture(architecture),
              m_mode(mode) { }

        bool start() override {
            if (m_initialized) return false;

            m_instruction = nullptr;
            auto mode = m_mode;
            if (m_endian == std::endian::little) {
                mode = cs_mode(u32(mode) | CS_MODE_LITTLE_ENDIAN);
            } else if (m_endian == std::endian::big) {
                mode = cs_mode(u32(mode) | CS_MODE_BIG_ENDIAN);
            }


            if (cs_open(CapstoneDisassembler::toCapstoneArchitecture(m_architecture), mode, &m_handle) != CS_ERR_OK) {
                return false;
            }

            cs_option(m_handle, CS_OPT_SKIPDATA, CS_OPT_ON);
            cs_option(m_handle, CS_OPT_SYNTAX, m_syntaxMode);

            m_instruction = cs_malloc(m_handle);

            m_initialized = true;
            return true;
        }

        void end() override {
            cs_free(m_instruction, 1);
            cs_close(&m_handle);

            m_initialized = false;
        }

        void drawSettings() override {
            // Endianness selection
            {
                int selection = [this] {
                    switch (m_endian) {
                        default:
                        case std::endian::little:
                            return 0;
                        case std::endian::big:
                            return 1;
                    }
                }();

                std::array options = {
                    fmt::format("{}:  {}", "hex.ui.common.endian"_lang, "hex.ui.common.little"_lang),
                    fmt::format("{}:  {}", "hex.ui.common.endian"_lang, "hex.ui.common.big"_lang)
                };

                if (ImGui::SliderInt("##endian", &selection, 0, options.size() - 1, options[selection].c_str(), ImGuiSliderFlags_NoInput)) {
                    switch (selection) {
                        default:
                        case 0:
                            m_endian = std::endian::little;
                            break;
                        case 1:
                            m_endian = std::endian::big;
                            break;
                    }
                }
            }

            {
                constexpr static std::pair<LangConst, cs_opt_value> Modes[] = {
                    { "hex.disassembler.view.disassembler.settings.syntax.default"_lang,     CS_OPT_SYNTAX_DEFAULT   },
                    { "hex.disassembler.view.disassembler.settings.syntax.intel"_lang,       CS_OPT_SYNTAX_INTEL     },
                    { "hex.disassembler.view.disassembler.settings.syntax.att"_lang,         CS_OPT_SYNTAX_ATT       },
                    { "hex.disassembler.view.disassembler.settings.syntax.masm"_lang,        CS_OPT_SYNTAX_MASM      },
                    { "hex.disassembler.view.disassembler.settings.syntax.motorola"_lang,    CS_OPT_SYNTAX_MOTOROLA  },
                };

                if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.syntax"_lang, Modes[m_syntaxModeIndex].first)) {
                    for (u32 i = 0; i < IM_ARRAYSIZE(Modes); i++) {
                        if (ImGui::Selectable(Modes[i].first))
                            m_syntaxModeIndex = i;
                    }
                    ImGui::EndCombo();
                }

                m_syntaxMode = cs_opt_value(Modes[m_syntaxModeIndex].second);
            }

            ImGui::Separator();
        }

        std::optional<ContentRegistry::Disassemblers::Instruction> disassemble(u64 imageBaseAddress, u64 instructionLoadAddress, u64 instructionDataAddress, std::span<const u8> code) override {
            auto ptr = code.data();
            auto size = code.size_bytes();

            if (!cs_disasm_iter(m_handle, &ptr, &size, &instructionLoadAddress, m_instruction)) {
                return std::nullopt;
            }

            ContentRegistry::Disassemblers::Instruction disassembly = { };
            disassembly.address     = m_instruction->address;
            disassembly.offset      = instructionDataAddress - imageBaseAddress;
            disassembly.size        = m_instruction->size;
            disassembly.mnemonic    = m_instruction->mnemonic;
            disassembly.operators   = m_instruction->op_str;

            for (u16 j = 0; j < m_instruction->size; j++)
                disassembly.bytes += fmt::format("{0:02X} ", m_instruction->bytes[j]);
            disassembly.bytes.pop_back();

            return disassembly;
        }

    private:
        BuiltinArchitecture m_architecture;
        csh m_handle = 0;
        cs_insn *m_instruction = nullptr;
        int m_syntaxModeIndex = 0;
        cs_opt_value m_syntaxMode = CS_OPT_SYNTAX_DEFAULT;

    protected:
        cs_mode m_mode = cs_mode(0);
        std::endian m_endian = std::endian::little;
        bool m_initialized = false;
    };

    class ArchitectureARM : public CapstoneArchitecture {
    public:
        ArchitectureARM(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::ARM, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            ImGui::RadioButton("hex.disassembler.view.disassembler.arm.arm"_lang, &m_armMode, CS_MODE_ARM);
            ImGui::SameLine();
            ImGui::RadioButton("hex.disassembler.view.disassembler.arm.thumb"_lang, &m_armMode, CS_MODE_THUMB);

            ImGui::RadioButton("hex.disassembler.view.disassembler.arm.default"_lang, &m_extraMode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("hex.disassembler.view.disassembler.arm.cortex_m"_lang, &m_extraMode, CS_MODE_MCLASS);
            ImGui::SameLine();
            ImGui::RadioButton("hex.disassembler.view.disassembler.arm.armv8"_lang, &m_extraMode, CS_MODE_V8);

            m_mode = cs_mode(m_armMode | m_extraMode);
        }

    private:
        int m_armMode = CS_MODE_ARM, m_extraMode = 0;
    };

    class ArchitectureARM64 : public CapstoneArchitecture {
    public:
        ArchitectureARM64(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::ARM64, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            #if CS_API_MAJOR >= 6
                ImGui::Checkbox("hex.disassembler.view.disassembler.arm64.apple_extensions"_lang, &m_appleExtensions);

                m_mode = cs_mode(m_appleExtensions ? CS_MODE_APPLE_PROPRIETARY : cs_mode(0));
            #endif
        }

    private:
        [[maybe_unused]] bool m_appleExtensions = false;
    };

    class ArchitectureMIPS : public CapstoneArchitecture {
    public:
        ArchitectureMIPS(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::MIPS, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            constexpr static std::pair<LangConst, cs_mode> Modes[] = {
                { "hex.disassembler.view.disassembler.mips.mips32"_lang,    CS_MODE_MIPS32 },
                { "hex.disassembler.view.disassembler.mips.mips64"_lang,    CS_MODE_MIPS64 },
            #if CS_API_MAJOR >= 6
                { "hex.disassembler.view.disassembler.mips.mips1"_lang,     CS_MODE_MIPS1 },
            #endif
                { "hex.disassembler.view.disassembler.mips.mips2"_lang,     CS_MODE_MIPS2 },
                { "hex.disassembler.view.disassembler.mips.mips3"_lang,     CS_MODE_MIPS3 },
            #if CS_API_MAJOR >= 6
                { "hex.disassembler.view.disassembler.mips.mips4"_lang,     CS_MODE_MIPS4 },
                { "hex.disassembler.view.disassembler.mips.mips5"_lang,     CS_MODE_MIPS5 },
                { "hex.disassembler.view.disassembler.mips.mips32r2"_lang,  CS_MODE_MIPS32R2 },
                { "hex.disassembler.view.disassembler.mips.mips32r3"_lang,  CS_MODE_MIPS32R3 },
                { "hex.disassembler.view.disassembler.mips.mips32r5"_lang,  CS_MODE_MIPS32R5 },
                { "hex.disassembler.view.disassembler.mips.mips64r2"_lang,  CS_MODE_MIPS64R2 },
                { "hex.disassembler.view.disassembler.mips.mips64r3"_lang,  CS_MODE_MIPS64R3 },
                { "hex.disassembler.view.disassembler.mips.mips64r5"_lang,  CS_MODE_MIPS64R5 },
                { "hex.disassembler.view.disassembler.mips.mips64r6"_lang,  CS_MODE_MIPS64R6 },
                { "hex.disassembler.view.disassembler.mips.octeon"_lang,    CS_MODE_OCTEON },
                { "hex.disassembler.view.disassembler.mips.octeonp"_lang,   CS_MODE_OCTEONP },
                { "hex.disassembler.view.disassembler.mips.nanomips"_lang,  CS_MODE_NANOMIPS },
                { "hex.disassembler.view.disassembler.mips.nms1"_lang,      CS_MODE_NMS1 },
                { "hex.disassembler.view.disassembler.mips.i7200"_lang,     CS_MODE_I7200 },
            #endif
            };

            if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, Modes[m_selectedMode].first)) {
                for (u32 i = 0; i < IM_ARRAYSIZE(Modes); i++) {
                    if (ImGui::Selectable(Modes[i].first))
                        m_selectedMode = i;
                }
                ImGui::EndCombo();
            }

            m_mode = cs_mode(Modes[m_selectedMode].second);
            
            #if CS_API_MAJOR >= 6
                ImGui::Checkbox("hex.disassembler.view.disassembler.mips.nofloat"_lang, &m_nofloats);
                ImGui::Checkbox("hex.disassembler.view.disassembler.mips.ptr64"_lang, &m_ptr64);

                m_mode = cs_mode(
                    (m_nofloats ? CS_MODE_MIPS_NOFLOAT : cs_mode(0)) |
                    (m_ptr64    ? CS_MODE_MIPS_PTR64   : cs_mode(0))
                );
            #endif

            ImGui::Checkbox("hex.disassembler.view.disassembler.mips.micro"_lang, &m_microMode);

            m_mode = cs_mode(m_mode | (m_microMode ? CS_MODE_MICRO : cs_mode(0)));
        }

    private:
        int m_selectedMode = 0;
        bool m_microMode = false;
        [[maybe_unused]] bool m_nofloats = false;
        [[maybe_unused]] bool m_ptr64 = false;
    };

    class ArchitectureX86 : public CapstoneArchitecture {
    public:
        ArchitectureX86(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::X86, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            ImGui::RadioButton("hex.disassembler.view.disassembler.16bit"_lang, &m_x86Mode, CS_MODE_16);
            ImGui::SameLine();
            ImGui::RadioButton("hex.disassembler.view.disassembler.32bit"_lang, &m_x86Mode, CS_MODE_32);
            ImGui::SameLine();
            ImGui::RadioButton("hex.disassembler.view.disassembler.64bit"_lang, &m_x86Mode, CS_MODE_64);

            m_mode = cs_mode(m_x86Mode);
        }

    private:
        int m_x86Mode = CS_MODE_32;
    };

    class ArchitecturePowerPC : public CapstoneArchitecture {
    public:
        ArchitecturePowerPC(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::POWERPC, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            ImGui::RadioButton("hex.disassembler.view.disassembler.32bit"_lang, &m_ppcMode, CS_MODE_32);
            ImGui::SameLine();
            ImGui::RadioButton("hex.disassembler.view.disassembler.64bit"_lang, &m_ppcMode, CS_MODE_64);

            ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.qpx"_lang, &m_qpx);

            ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.spe"_lang, &m_spe);
            ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.booke"_lang, &m_booke);

            m_mode = cs_mode(
                m_ppcMode |
                (m_qpx   ? CS_MODE_QPX   : cs_mode(0)) |
                (m_spe   ? CS_MODE_SPE   : cs_mode(0)) |
                (m_booke ? CS_MODE_BOOKE : cs_mode(0))
            );
            #if CS_API_MAJOR >= 6
                ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.pwr7"_lang,   &m_pwr7);
                ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.pwr8"_lang,   &m_pwr8);
                ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.pwr9"_lang,   &m_pwr9);
                ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.pwr10"_lang,  &m_pwr10);
                ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.aixos"_lang,  &m_aixos);
                ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.future"_lang, &m_future);
                ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.aixas"_lang,  &m_aixas);
                ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.msync"_lang,  &m_msync);

                m_mode = cs_mode(
                    u32(m_mode) |
                    (m_pwr7   ? CS_MODE_PWR7            : cs_mode(0)) |
                    (m_pwr8   ? CS_MODE_PWR8            : cs_mode(0)) |
                    (m_pwr9   ? CS_MODE_PWR9            : cs_mode(0)) |
                    (m_pwr10  ? CS_MODE_PWR10           : cs_mode(0)) |
                    (m_aixos  ? CS_MODE_AIX_OS          : cs_mode(0)) |
                    (m_future ? CS_MODE_PPC_ISA_FUTURE  : cs_mode(0)) |
                    (m_aixas  ? CS_MODE_MODERN_AIX_AS   : cs_mode(0)) |
                    (m_msync  ? CS_MODE_MSYNC           : cs_mode(0))
                );
            #endif
        }

    private:
        int m_ppcMode = CS_MODE_32;
        bool m_qpx = false;
        bool m_spe = false;
        bool m_booke = false;
        [[maybe_unused]] bool m_pwr7 = false;
        [[maybe_unused]] bool m_pwr8 = false;
        [[maybe_unused]] bool m_pwr9 = false;
        [[maybe_unused]] bool m_pwr10 = false;
        [[maybe_unused]] bool m_aixos = false;
        [[maybe_unused]] bool m_future = false;
        [[maybe_unused]] bool m_aixas = false;
        [[maybe_unused]] bool m_msync = false;
    };

    class ArchitectureSPARC : public CapstoneArchitecture {
    public:
        ArchitectureSPARC(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::SPARC, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            ImGui::Checkbox("hex.disassembler.view.disassembler.sparc.v9"_lang, &m_v9Mode);

            m_mode = cs_mode(m_v9Mode ? CS_MODE_V9 : cs_mode(0));
        }

    private:
        bool m_v9Mode = false;
    };

    class ArchitectureSystemZ : public CapstoneArchitecture {
    public:
        ArchitectureSystemZ(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::SYSTEMZ, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            #if CS_API_MAJOR >= 6
                constexpr static std::pair<LangConst, cs_mode> Modes[] = {
                    { "hex.disassembler.view.disassembler.systemz.arch8"_lang,      CS_MODE_SYSTEMZ_ARCH8},
                    { "hex.disassembler.view.disassembler.systemz.arch9"_lang,      CS_MODE_SYSTEMZ_ARCH9},
                    { "hex.disassembler.view.disassembler.systemz.arch10"_lang,     CS_MODE_SYSTEMZ_ARCH10},
                    { "hex.disassembler.view.disassembler.systemz.arch11"_lang,     CS_MODE_SYSTEMZ_ARCH11},
                    { "hex.disassembler.view.disassembler.systemz.arch12"_lang,     CS_MODE_SYSTEMZ_ARCH12},
                    { "hex.disassembler.view.disassembler.systemz.arch13"_lang,     CS_MODE_SYSTEMZ_ARCH13},
                    { "hex.disassembler.view.disassembler.systemz.arch14"_lang,     CS_MODE_SYSTEMZ_ARCH14},
                    { "hex.disassembler.view.disassembler.systemz.z10"_lang,        CS_MODE_SYSTEMZ_Z10},
                    { "hex.disassembler.view.disassembler.systemz.z196"_lang,       CS_MODE_SYSTEMZ_Z196},
                    { "hex.disassembler.view.disassembler.systemz.zec12"_lang,      CS_MODE_SYSTEMZ_ZEC12},
                    { "hex.disassembler.view.disassembler.systemz.z13"_lang,        CS_MODE_SYSTEMZ_Z13},
                    { "hex.disassembler.view.disassembler.systemz.z14"_lang,        CS_MODE_SYSTEMZ_Z14},
                    { "hex.disassembler.view.disassembler.systemz.z15"_lang,        CS_MODE_SYSTEMZ_Z15},
                    { "hex.disassembler.view.disassembler.systemz.z16"_lang,        CS_MODE_SYSTEMZ_Z16},
                    { "hex.disassembler.view.disassembler.systemz.generic"_lang,    CS_MODE_SYSTEMZ_GENERIC},
                };

                if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, Modes[m_selectedMode].first)) {
                    for (u32 i = 0; i < IM_ARRAYSIZE(Modes); i++) {
                        if (ImGui::Selectable(Modes[i].first))
                            m_selectedMode = i;
                    }
                    ImGui::EndCombo();
                }

                m_mode = cs_mode(Modes[m_selectedMode].second);
            #endif
        }

    private:
        [[maybe_unused]] int m_selectedMode = 0;
    };

    class ArchitectureXCore : public CapstoneArchitecture {
    public:
        ArchitectureXCore(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::XCORE, mode) {}
    };

    class ArchitectureM68K : public CapstoneArchitecture {
    public:
        ArchitectureM68K(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::M68K, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            constexpr static std::pair<LangConst, cs_mode> Modes[] = {
                { "hex.disassembler.view.disassembler.m68k.000"_lang, CS_MODE_M68K_000},
                { "hex.disassembler.view.disassembler.m68k.010"_lang, CS_MODE_M68K_010},
                { "hex.disassembler.view.disassembler.m68k.020"_lang, CS_MODE_M68K_020},
                { "hex.disassembler.view.disassembler.m68k.030"_lang, CS_MODE_M68K_030},
                { "hex.disassembler.view.disassembler.m68k.040"_lang, CS_MODE_M68K_040},
                { "hex.disassembler.view.disassembler.m68k.060"_lang, CS_MODE_M68K_060},
            };

            if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, Modes[m_selectedMode].first)) {
                for (u32 i = 0; i < IM_ARRAYSIZE(Modes); i++) {
                    if (ImGui::Selectable(Modes[i].first))
                        m_selectedMode = i;
                }
                ImGui::EndCombo();
            }

            m_mode = cs_mode(Modes[m_selectedMode].second);
        }

    private:
        int m_selectedMode = 0;
    };

    class ArchitectureTMS320C64X : public CapstoneArchitecture {
    public:
        ArchitectureTMS320C64X(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::TMS320C64X, mode) {}
    };
    class ArchitectureM680X : public CapstoneArchitecture {
    public:
        ArchitectureM680X(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::M680X, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            constexpr static std::pair<LangConst, cs_mode> Modes[] = {
                {"hex.disassembler.view.disassembler.m680x.6301"_lang,   CS_MODE_M680X_6301 },
                { "hex.disassembler.view.disassembler.m680x.6309"_lang,  CS_MODE_M680X_6309 },
                { "hex.disassembler.view.disassembler.m680x.6800"_lang,  CS_MODE_M680X_6800 },
                { "hex.disassembler.view.disassembler.m680x.6801"_lang,  CS_MODE_M680X_6801 },
                { "hex.disassembler.view.disassembler.m680x.6805"_lang,  CS_MODE_M680X_6805 },
                { "hex.disassembler.view.disassembler.m680x.6808"_lang,  CS_MODE_M680X_6808 },
                { "hex.disassembler.view.disassembler.m680x.6809"_lang,  CS_MODE_M680X_6809 },
                { "hex.disassembler.view.disassembler.m680x.6811"_lang,  CS_MODE_M680X_6811 },
                { "hex.disassembler.view.disassembler.m680x.cpu12"_lang, CS_MODE_M680X_CPU12},
                { "hex.disassembler.view.disassembler.m680x.hcs08"_lang, CS_MODE_M680X_HCS08},
            };

            if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, Modes[m_selectedMode].first)) {
                for (u32 i = 0; i < IM_ARRAYSIZE(Modes); i++) {
                    if (ImGui::Selectable(Modes[i].first))
                        m_selectedMode = i;
                }
                ImGui::EndCombo();
            }

            m_mode = cs_mode(Modes[m_selectedMode].second);
        }

    private:
        int m_selectedMode = 0;
    };

    class ArchitectureEVM : public CapstoneArchitecture {
    public:
        ArchitectureEVM(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::EVM, mode) {}
    };

    class ArchitectureWASM : public CapstoneArchitecture {
    public:
        ArchitectureWASM(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::WASM, mode) {}
    };

    class ArchitectureRISCV : public CapstoneArchitecture {
    public:
        ArchitectureRISCV(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::RISCV, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            ImGui::RadioButton("hex.disassembler.view.disassembler.32bit"_lang, &m_riscvMode, CS_MODE_RISCV32);
            ImGui::SameLine();
            ImGui::RadioButton("hex.disassembler.view.disassembler.64bit"_lang, &m_riscvMode, CS_MODE_RISCV64);

            ImGui::Checkbox("hex.disassembler.view.disassembler.riscv.compressed"_lang, &m_compressed);

            m_mode = cs_mode(m_riscvMode | (m_compressed ? CS_MODE_RISCVC : cs_mode(0)));
        }

    private:
        int m_riscvMode = CS_MODE_RISCV32;
        bool m_compressed = false;
    };

    class ArchitectureMOS65XX : public CapstoneArchitecture {
    public:
        ArchitectureMOS65XX(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::MOS65XX, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            constexpr static std::pair<LangConst, cs_mode> Modes[] = {
                {"hex.disassembler.view.disassembler.mos65xx.6502"_lang,           CS_MODE_MOS65XX_6502         },
                { "hex.disassembler.view.disassembler.mos65xx.65c02"_lang,         CS_MODE_MOS65XX_65C02        },
                { "hex.disassembler.view.disassembler.mos65xx.w65c02"_lang,        CS_MODE_MOS65XX_W65C02       },
                { "hex.disassembler.view.disassembler.mos65xx.65816"_lang,         CS_MODE_MOS65XX_65816        },
                { "hex.disassembler.view.disassembler.mos65xx.65816_long_m"_lang,  CS_MODE_MOS65XX_65816_LONG_M },
                { "hex.disassembler.view.disassembler.mos65xx.65816_long_x"_lang,  CS_MODE_MOS65XX_65816_LONG_X },
                { "hex.disassembler.view.disassembler.mos65xx.65816_long_mx"_lang, CS_MODE_MOS65XX_65816_LONG_MX},
            };

            if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, Modes[m_selectedMode].first)) {
                for (u32 i = 0; i < IM_ARRAYSIZE(Modes); i++) {
                    if (ImGui::Selectable(Modes[i].first))
                        m_selectedMode = i;
                }
                ImGui::EndCombo();
            }

            m_mode = cs_mode(Modes[m_selectedMode].second);
        }

    private:
        int m_selectedMode = 0;
    };

    class ArchitectureBPF : public CapstoneArchitecture {
    public:
        ArchitectureBPF(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::BPF, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            ImGui::RadioButton("hex.disassembler.view.disassembler.bpf.classic"_lang, &m_bpfMode, CS_MODE_BPF_CLASSIC);
            ImGui::SameLine();
            ImGui::RadioButton("hex.disassembler.view.disassembler.bpf.extended"_lang, &m_bpfMode, CS_MODE_BPF_EXTENDED);

            m_mode = cs_mode(m_bpfMode);
        }

    private:
        int m_bpfMode = CS_MODE_BPF_CLASSIC;
    };

    class ArchitectureSuperH : public CapstoneArchitecture {
    public:
        ArchitectureSuperH(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::SUPERH, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            constexpr static std::pair<LangConst, cs_mode> Modes[] = {
                { "hex.disassembler.view.disassembler.sh.sh2"_lang, CS_MODE_SH2 },
                { "hex.disassembler.view.disassembler.sh.sh2a"_lang, CS_MODE_SH2A },
                { "hex.disassembler.view.disassembler.sh.sh3"_lang, CS_MODE_SH3 },
                { "hex.disassembler.view.disassembler.sh.sh4"_lang, CS_MODE_SH4 },
                { "hex.disassembler.view.disassembler.sh.sh4a"_lang, CS_MODE_SH4A },
            };

            if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, Modes[m_selectedMode].first)) {
                for (u32 i = 0; i < IM_ARRAYSIZE(Modes); i++) {
                    if (ImGui::Selectable(Modes[i].first))
                        m_selectedMode = i;
                }
                ImGui::EndCombo();
            }

            ImGui::Checkbox("hex.disassembler.view.disassembler.sh.fpu"_lang, &m_fpu);
            ImGui::SameLine();
            ImGui::Checkbox("hex.disassembler.view.disassembler.sh.dsp"_lang, &m_dsp);

            m_mode = cs_mode(Modes[m_selectedMode].second | (m_fpu ? CS_MODE_SHFPU : cs_mode(0)) | (m_dsp ? CS_MODE_SHDSP : cs_mode(0)));
        }

    private:
        int m_selectedMode = 0;
        bool m_fpu = false;
        bool m_dsp = false;
    };

    class ArchitectureTricore : public CapstoneArchitecture {
    public:
        ArchitectureTricore(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::TRICORE, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            constexpr static std::pair<LangConst, cs_mode> Modes[] = {
                { "hex.disassembler.view.disassembler.tricore.110"_lang, CS_MODE_TRICORE_110 },
                { "hex.disassembler.view.disassembler.tricore.120"_lang, CS_MODE_TRICORE_120 },
                { "hex.disassembler.view.disassembler.tricore.130"_lang, CS_MODE_TRICORE_130 },
                { "hex.disassembler.view.disassembler.tricore.131"_lang, CS_MODE_TRICORE_131 },
                { "hex.disassembler.view.disassembler.tricore.160"_lang, CS_MODE_TRICORE_160 },
                { "hex.disassembler.view.disassembler.tricore.161"_lang, CS_MODE_TRICORE_161 },
                { "hex.disassembler.view.disassembler.tricore.162"_lang, CS_MODE_TRICORE_162 },
            };

            if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, Modes[m_selectedMode].first)) {
                for (u32 i = 0; i < IM_ARRAYSIZE(Modes); i++) {
                    if (ImGui::Selectable(Modes[i].first))
                        m_selectedMode = i;
                }
                ImGui::EndCombo();
            }

            m_mode = cs_mode(Modes[m_selectedMode].second);
        }

    private:
        int m_selectedMode = 0;
    };

#if CS_API_MAJOR >= 6

    class ArchitectureAlpha : public CapstoneArchitecture {
    public:
        ArchitectureAlpha(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::ALPHA, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();
        }
    };

    class ArchitectureHPPA : public CapstoneArchitecture {
    public:
        ArchitectureHPPA(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::HPPA, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            constexpr static std::pair<LangConst, cs_mode> Modes[] = {
                { "hex.disassembler.view.disassembler.hppa.1.1"_lang, CS_MODE_HPPA_11 },
                { "hex.disassembler.view.disassembler.hppa.2.0"_lang, CS_MODE_HPPA_20 },
                { "hex.disassembler.view.disassembler.hppa.2.0w"_lang, CS_MODE_HPPA_20W },
            };

            if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, Modes[m_selectedMode].first)) {
                for (u32 i = 0; i < IM_ARRAYSIZE(Modes); i++) {
                    if (ImGui::Selectable(Modes[i].first))
                        m_selectedMode = i;
                }
                ImGui::EndCombo();
            }

            m_mode = cs_mode(Modes[m_selectedMode].second);
        }

    private:
        int m_selectedMode = 0;
    };

    class ArchitectureLoongArch : public CapstoneArchitecture {
    public:
        ArchitectureLoongArch(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::LOONGARCH, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            ImGui::RadioButton("hex.disassembler.view.disassembler.32bit"_lang, &m_mode, CS_MODE_LOONGARCH32);
            ImGui::SameLine();
            ImGui::RadioButton("hex.disassembler.view.disassembler.64bit"_lang, &m_mode, CS_MODE_LOONGARCH64);
        }

        int m_mode = CS_MODE_LOONGARCH64;
    };

    class ArchitectureXtensa : public CapstoneArchitecture {
    public:
        ArchitectureXtensa(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::XTENSA, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            constexpr static std::pair<LangConst, cs_mode> Modes[] = {
                { "hex.disassembler.view.disassembler.xtensa.esp32"_lang, CS_MODE_XTENSA_ESP32 },
                { "hex.disassembler.view.disassembler.xtensa.esp32s2"_lang, CS_MODE_XTENSA_ESP32S2 },
                { "hex.disassembler.view.disassembler.xtensa.esp8266"_lang, CS_MODE_XTENSA_ESP8266 },
            };

            if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, Modes[m_selectedMode].first)) {
                for (u32 i = 0; i < IM_ARRAYSIZE(Modes); i++) {
                    if (ImGui::Selectable(Modes[i].first))
                        m_selectedMode = i;
                }
                ImGui::EndCombo();
            }

            m_mode = cs_mode(Modes[m_selectedMode].second);
        }

    private:
        int m_selectedMode = 0;
    };

    class ArchitectureArc : public CapstoneArchitecture {
    public:
        ArchitectureArc(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::ARC, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();
        }
    };

#endif

    void registerCapstoneArchitectures() {
        ContentRegistry::Disassemblers::add<ArchitectureARM>();
        ContentRegistry::Disassemblers::add<ArchitectureARM64>();
        ContentRegistry::Disassemblers::add<ArchitectureMIPS>();
        ContentRegistry::Disassemblers::add<ArchitectureX86>();
        ContentRegistry::Disassemblers::add<ArchitecturePowerPC>();
        ContentRegistry::Disassemblers::add<ArchitectureSPARC>();
        ContentRegistry::Disassemblers::add<ArchitectureSystemZ>();
        ContentRegistry::Disassemblers::add<ArchitectureXCore>();
        ContentRegistry::Disassemblers::add<ArchitectureM68K>();
        ContentRegistry::Disassemblers::add<ArchitectureTMS320C64X>();
        ContentRegistry::Disassemblers::add<ArchitectureM680X>();
        ContentRegistry::Disassemblers::add<ArchitectureEVM>();
        ContentRegistry::Disassemblers::add<ArchitectureWASM>();
        ContentRegistry::Disassemblers::add<ArchitectureRISCV>();
        ContentRegistry::Disassemblers::add<ArchitectureMOS65XX>();
        ContentRegistry::Disassemblers::add<ArchitectureBPF>();
        ContentRegistry::Disassemblers::add<ArchitectureSuperH>();
        ContentRegistry::Disassemblers::add<ArchitectureTricore>();

        #if CS_API_MAJOR >= 6
            ContentRegistry::Disassemblers::add<ArchitectureAlpha>();
            ContentRegistry::Disassemblers::add<ArchitectureHPPA>();
            ContentRegistry::Disassemblers::add<ArchitectureLoongArch>();
            ContentRegistry::Disassemblers::add<ArchitectureXtensa>();
            ContentRegistry::Disassemblers::add<ArchitectureArc>();
        #endif
    }

}
