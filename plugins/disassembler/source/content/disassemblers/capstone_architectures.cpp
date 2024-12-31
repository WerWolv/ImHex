#include <hex/api/content_registry.hpp>

#include <content/helpers/disassembler.hpp>
#include <hex/helpers/fmt.hpp>

#include <imgui.h>

namespace hex::plugin::disasm {

    class CapstoneArchitecture : public ContentRegistry::Disassembler::Architecture {
    public:
        explicit CapstoneArchitecture(BuiltinArchitecture architecture, cs_mode mode = cs_mode(0))
            : Architecture(CapstoneDisassembler::ArchitectureNames[u32(architecture)]),
              m_architecture(architecture),
              m_mode(mode) { }

        bool start() override {
            if (m_initialized) return false;

            m_instruction = nullptr;
            auto mode = m_mode;
            if (m_endian == true) {
                mode = cs_mode(u32(mode) | CS_MODE_LITTLE_ENDIAN);
            } else {
                mode = cs_mode(u32(mode) | CS_MODE_BIG_ENDIAN);
            }

            if (cs_open(CapstoneDisassembler::toCapstoneArchitecture(m_architecture), mode, &m_handle) != CS_ERR_OK) {
                return false;
            }

            cs_option(m_handle, CS_OPT_SKIPDATA, CS_OPT_ON);

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
            ImGui::RadioButton("hex.ui.common.little_endian"_lang, &m_endian, true);
            ImGui::SameLine();
            ImGui::RadioButton("hex.ui.common.big_endian"_lang, &m_endian, false);
            ImGui::NewLine();
        }

        std::optional<ContentRegistry::Disassembler::Instruction> disassemble(u64 imageBaseAddress, u64 instructionLoadAddress, u64 instructionDataAddress, std::span<const u8> code) override {
            auto ptr = code.data();
            auto size = code.size_bytes();

            if (!cs_disasm_iter(m_handle, &ptr, &size, &instructionLoadAddress, m_instruction)) {
                return std::nullopt;
            }

            ContentRegistry::Disassembler::Instruction disassembly = { };
            disassembly.address     = m_instruction->address;
            disassembly.offset      = instructionDataAddress - imageBaseAddress;
            disassembly.size        = m_instruction->size;
            disassembly.mnemonic    = m_instruction->mnemonic;
            disassembly.operators   = m_instruction->op_str;

            for (u16 j = 0; j < m_instruction->size; j++)
                disassembly.bytes += hex::format("{0:02X} ", m_instruction->bytes[j]);
            disassembly.bytes.pop_back();

            return disassembly;
        }

    private:
        BuiltinArchitecture m_architecture;
        csh m_handle = 0;
        cs_insn *m_instruction = nullptr;

    protected:
        cs_mode m_mode = cs_mode(0);
        int m_endian = true;
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
        }
    };

    class ArchitectureMIPS : public CapstoneArchitecture {
    public:
        ArchitectureMIPS(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::MIPS, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            ImGui::RadioButton("hex.disassembler.view.disassembler.mips.mips32"_lang, &m_mipsMode, CS_MODE_MIPS32);
            ImGui::SameLine();
            ImGui::RadioButton("hex.disassembler.view.disassembler.mips.mips64"_lang, &m_mipsMode, CS_MODE_MIPS64);
            ImGui::SameLine();
            ImGui::RadioButton("hex.disassembler.view.disassembler.mips.mips32R6"_lang, &m_mipsMode, CS_MODE_MIPS32R6);

            ImGui::RadioButton("hex.disassembler.view.disassembler.mips.mips2"_lang, &m_mipsMode, CS_MODE_MIPS2);
            ImGui::SameLine();
            ImGui::RadioButton("hex.disassembler.view.disassembler.mips.mips3"_lang, &m_mipsMode, CS_MODE_MIPS3);

            ImGui::Checkbox("hex.disassembler.view.disassembler.mips.micro"_lang, &m_microMode);

            m_mode = cs_mode(m_mipsMode | (m_microMode ? CS_MODE_MICRO : cs_mode(0)));
        }

    private:
        int m_mipsMode = CS_MODE_MIPS32;
        bool m_microMode = false;
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
        ArchitecturePowerPC(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::PPC, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            ImGui::RadioButton("hex.disassembler.view.disassembler.32bit"_lang, &m_ppcMode, CS_MODE_32);
            ImGui::SameLine();
            ImGui::RadioButton("hex.disassembler.view.disassembler.64bit"_lang, &m_ppcMode, CS_MODE_64);

            ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.qpx"_lang, &m_qpx);

            #if CS_API_MAJOR >= 5
                ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.spe"_lang, &m_spe);
                ImGui::Checkbox("hex.disassembler.view.disassembler.ppc.booke"_lang, &m_booke);

                m_mode = cs_mode(m_ppcMode | (m_qpx ? CS_MODE_QPX : cs_mode(0)) | (m_spe ? CS_MODE_SPE : cs_mode(0)) | (m_booke ? CS_MODE_BOOKE : cs_mode(0)));
            #else
                m_mode = cs_mode(mode | (qpx ? CS_MODE_QPX : cs_mode(0)));
            #endif
        }

    private:
        int m_ppcMode = CS_MODE_32;
        bool m_qpx = false;
        bool m_spe = false;
        bool m_booke = false;
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
        ArchitectureSystemZ(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::SYSZ, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();
        }
    };

    class ArchitectureXCore : public CapstoneArchitecture {
    public:
        ArchitectureXCore(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::XCORE, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();
        }
    };

    class ArchitectureM68K : public CapstoneArchitecture {
    public:
        ArchitectureM68K(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::M68K, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            std::pair<const char *, cs_mode> modes[] = {
                {"hex.disassembler.view.disassembler.m68k.000"_lang,  CS_MODE_M68K_000},
                { "hex.disassembler.view.disassembler.m68k.010"_lang, CS_MODE_M68K_010},
                { "hex.disassembler.view.disassembler.m68k.020"_lang, CS_MODE_M68K_020},
                { "hex.disassembler.view.disassembler.m68k.030"_lang, CS_MODE_M68K_030},
                { "hex.disassembler.view.disassembler.m68k.040"_lang, CS_MODE_M68K_040},
                { "hex.disassembler.view.disassembler.m68k.060"_lang, CS_MODE_M68K_060},
            };

            if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, modes[m_selectedMode].first)) {
                for (u32 i = 0; i < IM_ARRAYSIZE(modes); i++) {
                    if (ImGui::Selectable(modes[i].first))
                        m_selectedMode = i;
                }
                ImGui::EndCombo();
            }

            m_mode = cs_mode(modes[m_selectedMode].second);
        }

    private:
        int m_selectedMode = 0;
    };

    class ArchitectureTMS320C64X : public CapstoneArchitecture {
    public:
        ArchitectureTMS320C64X(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::TMS320C64X, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();
        }
    };
    class ArchitectureM680X : public CapstoneArchitecture {
    public:
        ArchitectureM680X(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::M680X, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();

            std::pair<const char *, cs_mode> modes[] = {
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

            if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, modes[m_selectedMode].first)) {
                for (u32 i = 0; i < IM_ARRAYSIZE(modes); i++) {
                    if (ImGui::Selectable(modes[i].first))
                        m_selectedMode = i;
                }
                ImGui::EndCombo();
            }

            m_mode = cs_mode(modes[m_selectedMode].second);
        }

    private:
        int m_selectedMode = 0;
    };

    class ArchitectureEVM : public CapstoneArchitecture {
    public:
        ArchitectureEVM(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::EVM, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();
        }
    };

#if CS_API_MAJOR >= 5

    class ArchitectureWASM : public CapstoneArchitecture {
    public:
        ArchitectureWASM(cs_mode mode = cs_mode(0)) : CapstoneArchitecture(BuiltinArchitecture::WASM, mode) {}

        void drawSettings() override {
            CapstoneArchitecture::drawSettings();
        }
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

            std::pair<const char *, cs_mode> modes[] = {
                {"hex.disassembler.view.disassembler.mos65xx.6502"_lang,           CS_MODE_MOS65XX_6502         },
                { "hex.disassembler.view.disassembler.mos65xx.65c02"_lang,         CS_MODE_MOS65XX_65C02        },
                { "hex.disassembler.view.disassembler.mos65xx.w65c02"_lang,        CS_MODE_MOS65XX_W65C02       },
                { "hex.disassembler.view.disassembler.mos65xx.65816"_lang,         CS_MODE_MOS65XX_65816        },
                { "hex.disassembler.view.disassembler.mos65xx.65816_long_m"_lang,  CS_MODE_MOS65XX_65816_LONG_M },
                { "hex.disassembler.view.disassembler.mos65xx.65816_long_x"_lang,  CS_MODE_MOS65XX_65816_LONG_X },
                { "hex.disassembler.view.disassembler.mos65xx.65816_long_mx"_lang, CS_MODE_MOS65XX_65816_LONG_MX},
            };

            if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, modes[m_selectedMode].first)) {
                for (u32 i = 0; i < IM_ARRAYSIZE(modes); i++) {
                    if (ImGui::Selectable(modes[i].first))
                        m_selectedMode = i;
                }
                ImGui::EndCombo();
            }

            m_mode = cs_mode(modes[m_selectedMode].second);
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

            std::pair<const char*, cs_mode> modes[] = {
                { "hex.disassembler.view.disassembler.sh.sh2"_lang, CS_MODE_SH2 },
                { "hex.disassembler.view.disassembler.sh.sh2a"_lang, CS_MODE_SH2A },
                { "hex.disassembler.view.disassembler.sh.sh3"_lang, CS_MODE_SH3 },
                { "hex.disassembler.view.disassembler.sh.sh4"_lang, CS_MODE_SH4 },
                { "hex.disassembler.view.disassembler.sh.sh4a"_lang, CS_MODE_SH4A },
            };

            if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, modes[m_selectedMode].first)) {
                for (u32 i = 0; i < IM_ARRAYSIZE(modes); i++) {
                    if (ImGui::Selectable(modes[i].first))
                        m_selectedMode = i;
                }
                ImGui::EndCombo();
            }

            ImGui::Checkbox("hex.disassembler.view.disassembler.sh.fpu"_lang, &m_fpu);
            ImGui::SameLine();
            ImGui::Checkbox("hex.disassembler.view.disassembler.sh.dsp"_lang, &m_dsp);

            m_mode = cs_mode(modes[m_selectedMode].second | (m_fpu ? CS_MODE_SHFPU : cs_mode(0)) | (m_dsp ? CS_MODE_SHDSP : cs_mode(0)));
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

            std::pair<const char*, cs_mode> modes[] = {
                { "hex.disassembler.view.disassembler.tricore.110"_lang, CS_MODE_TRICORE_110 },
                { "hex.disassembler.view.disassembler.tricore.120"_lang, CS_MODE_TRICORE_120 },
                { "hex.disassembler.view.disassembler.tricore.130"_lang, CS_MODE_TRICORE_130 },
                { "hex.disassembler.view.disassembler.tricore.131"_lang, CS_MODE_TRICORE_131 },
                { "hex.disassembler.view.disassembler.tricore.160"_lang, CS_MODE_TRICORE_160 },
                { "hex.disassembler.view.disassembler.tricore.161"_lang, CS_MODE_TRICORE_161 },
                { "hex.disassembler.view.disassembler.tricore.162"_lang, CS_MODE_TRICORE_162 },
            };

            if (ImGui::BeginCombo("hex.disassembler.view.disassembler.settings.mode"_lang, modes[m_selectedMode].first)) {
                for (u32 i = 0; i < IM_ARRAYSIZE(modes); i++) {
                    if (ImGui::Selectable(modes[i].first))
                        m_selectedMode = i;
                }
                ImGui::EndCombo();
            }

            m_mode = cs_mode(modes[m_selectedMode].second);
        }

    private:
        int m_selectedMode = 0;
    };

#endif

    void registerCapstoneArchitectures() {
        ContentRegistry::Disassembler::add<ArchitectureARM>();
        ContentRegistry::Disassembler::add<ArchitectureARM64>();
        ContentRegistry::Disassembler::add<ArchitectureMIPS>();
        ContentRegistry::Disassembler::add<ArchitectureX86>();
        ContentRegistry::Disassembler::add<ArchitecturePowerPC>();
        ContentRegistry::Disassembler::add<ArchitectureSPARC>();
        ContentRegistry::Disassembler::add<ArchitectureSystemZ>();
        ContentRegistry::Disassembler::add<ArchitectureXCore>();
        ContentRegistry::Disassembler::add<ArchitectureM68K>();
        ContentRegistry::Disassembler::add<ArchitectureTMS320C64X>();
        ContentRegistry::Disassembler::add<ArchitectureM680X>();
        ContentRegistry::Disassembler::add<ArchitectureEVM>();

        #if CS_API_MAJOR >= 5
            ContentRegistry::Disassembler::add<ArchitectureWASM>();
            ContentRegistry::Disassembler::add<ArchitectureRISCV>();
            ContentRegistry::Disassembler::add<ArchitectureMOS65XX>();
            ContentRegistry::Disassembler::add<ArchitectureBPF>();
            ContentRegistry::Disassembler::add<ArchitectureSuperH>();
            ContentRegistry::Disassembler::add<ArchitectureTricore>();
        #endif
    }

}
