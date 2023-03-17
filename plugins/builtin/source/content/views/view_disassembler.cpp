#include "content/views/view_disassembler.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>

#include <cstring>
#include <thread>

using namespace std::literals::string_literals;

namespace hex::plugin::builtin {

    ViewDisassembler::ViewDisassembler() : View("hex.builtin.view.disassembler.name") {
        EventManager::subscribe<EventProviderDeleted>(this, [this](const auto*) {
            this->m_disassembly.clear();
        });
    }

    ViewDisassembler::~ViewDisassembler() {
        EventManager::unsubscribe<EventDataChanged>(this);
        EventManager::unsubscribe<EventRegionSelected>(this);
        EventManager::unsubscribe<EventProviderDeleted>(this);
    }

    void ViewDisassembler::disassemble() {
        this->m_disassembly.clear();

        this->m_disassemblerTask = TaskManager::createTask("hex.builtin.view.disassembler.disassembling", this->m_codeRegion.getSize(), [this](auto &task) {
            csh capstoneHandle;
            cs_insn *instructions = nullptr;

            cs_mode mode = this->m_mode;

            if (cs_open(Disassembler::toCapstoneArchitecture(this->m_architecture), mode, &capstoneHandle) == CS_ERR_OK) {

                cs_option(capstoneHandle, CS_OPT_SKIPDATA, CS_OPT_ON);

                auto provider = ImHexApi::Provider::get();
                std::vector<u8> buffer(2048, 0x00);
                size_t size = this->m_codeRegion.getSize();

                for (u64 address = 0; address < size; address += 2048) {
                    task.update(address);

                    size_t bufferSize = std::min(u64(2048), (size - address));
                    provider->read(this->m_codeRegion.getStartAddress() + address, buffer.data(), bufferSize);

                    size_t instructionCount = cs_disasm(capstoneHandle, buffer.data(), bufferSize, this->m_baseAddress + address, 0, &instructions);
                    if (instructionCount == 0)
                        break;

                    this->m_disassembly.reserve(this->m_disassembly.size() + instructionCount);

                    u64 usedBytes = 0;
                    for (u32 i = 0; i < instructionCount; i++) {
                        const auto &instr       = instructions[i];
                        Disassembly disassembly = { };
                        disassembly.address     = instr.address;
                        disassembly.offset      = this->m_codeRegion.getStartAddress() + address + usedBytes;
                        disassembly.size        = instr.size;
                        disassembly.mnemonic    = instr.mnemonic;
                        disassembly.operators   = instr.op_str;

                        for (u16 j = 0; j < instr.size; j++)
                            disassembly.bytes += hex::format("{0:02X} ", instr.bytes[j]);
                        disassembly.bytes.pop_back();

                        this->m_disassembly.push_back(disassembly);

                        usedBytes += instr.size;
                    }

                    if (instructionCount < bufferSize)
                        address -= (bufferSize - usedBytes);

                    cs_free(instructions, instructionCount);
                }

                cs_close(&capstoneHandle);
            }
        });
    }

    void ViewDisassembler::drawContent() {

        if (ImGui::Begin(View::toWindowName("hex.builtin.view.disassembler.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            auto provider = ImHexApi::Provider::get();
            if (ImHexApi::Provider::isValid() && provider->isReadable()) {
                ImGui::TextUnformatted("hex.builtin.view.disassembler.position"_lang);
                ImGui::Separator();

                ImGui::InputHexadecimal("hex.builtin.view.disassembler.base"_lang, &this->m_baseAddress, ImGuiInputTextFlags_CharsHexadecimal);

                ui::regionSelectionPicker(&this->m_range);
                switch (this->m_range) {
                    case ui::SelectedRegion::Selection: {
                        auto region = ImHexApi::HexEditor::getSelection();
                        if (region.has_value()) {
                            this->m_codeRegion = region.value();
                        }
                    }
                        break;
                    case ui::SelectedRegion::EntireData: {
                        auto base = provider->getBaseAddress();
                        this->m_codeRegion = { base, base + provider->getActualSize() - 1 };
                    }
                    break;
                }

                if (ImGui::IsItemEdited()) {
                    // Force execution of Region Selection Event
                    ImHexApi::HexEditor::setSelection(0, 0);
                }

                ImGui::Header("hex.builtin.common.settings"_lang);

                if (ImGui::Combo("hex.builtin.view.disassembler.arch"_lang, reinterpret_cast<int *>(&this->m_architecture), Disassembler::ArchitectureNames, Disassembler::getArchitectureSupportedCount()))
                    this->m_mode = cs_mode(0);


                if (ImGui::BeginChild("modes", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 6), true, ImGuiWindowFlags_AlwaysAutoResize)) {

                    static int littleEndian = true;
                    ImGui::RadioButton("hex.builtin.common.little_endian"_lang, &littleEndian, true);
                    ImGui::SameLine();
                    ImGui::RadioButton("hex.builtin.common.big_endian"_lang, &littleEndian, false);

                    ImGui::NewLine();

                    switch (this->m_architecture) {
                        case Architecture::ARM:
                            {
                                static int mode = CS_MODE_ARM;
                                ImGui::RadioButton("hex.builtin.view.disassembler.arm.arm"_lang, &mode, CS_MODE_ARM);
                                ImGui::SameLine();
                                ImGui::RadioButton("hex.builtin.view.disassembler.arm.thumb"_lang, &mode, CS_MODE_THUMB);

                                static int extraMode = 0;
                                ImGui::RadioButton("hex.builtin.view.disassembler.arm.default"_lang, &extraMode, 0);
                                ImGui::SameLine();
                                ImGui::RadioButton("hex.builtin.view.disassembler.arm.cortex_m"_lang, &extraMode, CS_MODE_MCLASS);
                                ImGui::SameLine();
                                ImGui::RadioButton("hex.builtin.view.disassembler.arm.armv8"_lang, &extraMode, CS_MODE_V8);

                                this->m_mode = cs_mode(mode | extraMode);
                            }
                            break;
                        case Architecture::MIPS:
                            {
                                static int mode = CS_MODE_MIPS32;
                                ImGui::RadioButton("hex.builtin.view.disassembler.mips.mips32"_lang, &mode, CS_MODE_MIPS32);
                                ImGui::SameLine();
                                ImGui::RadioButton("hex.builtin.view.disassembler.mips.mips64"_lang, &mode, CS_MODE_MIPS64);
                                ImGui::SameLine();
                                ImGui::RadioButton("hex.builtin.view.disassembler.mips.mips32R6"_lang, &mode, CS_MODE_MIPS32R6);

                                ImGui::RadioButton("hex.builtin.view.disassembler.mips.mips2"_lang, &mode, CS_MODE_MIPS2);
                                ImGui::SameLine();
                                ImGui::RadioButton("hex.builtin.view.disassembler.mips.mips3"_lang, &mode, CS_MODE_MIPS3);

                                static bool microMode;
                                ImGui::Checkbox("hex.builtin.view.disassembler.mips.micro"_lang, &microMode);

                                this->m_mode = cs_mode(mode | (microMode ? CS_MODE_MICRO : cs_mode(0)));
                            }
                            break;
                        case Architecture::X86:
                            {
                                static int mode = CS_MODE_32;
                                ImGui::RadioButton("hex.builtin.view.disassembler.16bit"_lang, &mode, CS_MODE_16);
                                ImGui::SameLine();
                                ImGui::RadioButton("hex.builtin.view.disassembler.32bit"_lang, &mode, CS_MODE_32);
                                ImGui::SameLine();
                                ImGui::RadioButton("hex.builtin.view.disassembler.64bit"_lang, &mode, CS_MODE_64);

                                this->m_mode = cs_mode(mode);
                            }
                            break;
                        case Architecture::PPC:
                            {
                                static int mode = CS_MODE_32;
                                ImGui::RadioButton("hex.builtin.view.disassembler.32bit"_lang, &mode, CS_MODE_32);
                                ImGui::SameLine();
                                ImGui::RadioButton("hex.builtin.view.disassembler.64bit"_lang, &mode, CS_MODE_64);

                                static bool qpx = false;
                                ImGui::Checkbox("hex.builtin.view.disassembler.ppc.qpx"_lang, &qpx);
                                static bool spe = false;
                                ImGui::Checkbox("hex.builtin.view.disassembler.ppc.spe"_lang, &spe);
                                static bool booke = false;
                                ImGui::Checkbox("hex.builtin.view.disassembler.ppc.booke"_lang, &booke);

                                this->m_mode = cs_mode(mode | (qpx ? CS_MODE_QPX : cs_mode(0)) | (spe ? CS_MODE_SPE : cs_mode(0)) | (booke ? CS_MODE_BOOKE : cs_mode(0)));
                            }
                            break;
                        case Architecture::SPARC:
                            {
                                static bool v9Mode = false;
                                ImGui::Checkbox("hex.builtin.view.disassembler.sparc.v9"_lang, &v9Mode);

                                this->m_mode = cs_mode(v9Mode ? CS_MODE_V9 : cs_mode(0));
                            }
                            break;
                        case Architecture::RISCV:
                            {
                                static int mode = CS_MODE_RISCV32;
                                ImGui::RadioButton("hex.builtin.view.disassembler.32bit"_lang, &mode, CS_MODE_RISCV32);
                                ImGui::SameLine();
                                ImGui::RadioButton("hex.builtin.view.disassembler.64bit"_lang, &mode, CS_MODE_RISCV64);

                                static bool compressed = false;
                                ImGui::Checkbox("hex.builtin.view.disassembler.riscv.compressed"_lang, &compressed);

                                this->m_mode = cs_mode(mode | (compressed ? CS_MODE_RISCVC : cs_mode(0)));
                            }
                            break;
                        case Architecture::M68K:
                            {
                                static int selectedMode = 0;

                                std::pair<const char *, cs_mode> modes[] = {
                                    {"hex.builtin.view.disassembler.m68k.000"_lang,  CS_MODE_M68K_000},
                                    { "hex.builtin.view.disassembler.m68k.010"_lang, CS_MODE_M68K_010},
                                    { "hex.builtin.view.disassembler.m68k.020"_lang, CS_MODE_M68K_020},
                                    { "hex.builtin.view.disassembler.m68k.030"_lang, CS_MODE_M68K_030},
                                    { "hex.builtin.view.disassembler.m68k.040"_lang, CS_MODE_M68K_040},
                                    { "hex.builtin.view.disassembler.m68k.060"_lang, CS_MODE_M68K_060},
                                };

                                if (ImGui::BeginCombo("hex.builtin.view.disassembler.settings.mode"_lang, modes[selectedMode].first)) {
                                    for (u32 i = 0; i < IM_ARRAYSIZE(modes); i++) {
                                        if (ImGui::Selectable(modes[i].first))
                                            selectedMode = i;
                                    }
                                    ImGui::EndCombo();
                                }

                                this->m_mode = cs_mode(modes[selectedMode].second);
                            }
                            break;
                        case Architecture::M680X:
                            {
                                static int selectedMode = 0;

                                std::pair<const char *, cs_mode> modes[] = {
                                    {"hex.builtin.view.disassembler.m680x.6301"_lang,   CS_MODE_M680X_6301 },
                                    { "hex.builtin.view.disassembler.m680x.6309"_lang,  CS_MODE_M680X_6309 },
                                    { "hex.builtin.view.disassembler.m680x.6800"_lang,  CS_MODE_M680X_6800 },
                                    { "hex.builtin.view.disassembler.m680x.6801"_lang,  CS_MODE_M680X_6801 },
                                    { "hex.builtin.view.disassembler.m680x.6805"_lang,  CS_MODE_M680X_6805 },
                                    { "hex.builtin.view.disassembler.m680x.6808"_lang,  CS_MODE_M680X_6808 },
                                    { "hex.builtin.view.disassembler.m680x.6809"_lang,  CS_MODE_M680X_6809 },
                                    { "hex.builtin.view.disassembler.m680x.6811"_lang,  CS_MODE_M680X_6811 },
                                    { "hex.builtin.view.disassembler.m680x.cpu12"_lang, CS_MODE_M680X_CPU12},
                                    { "hex.builtin.view.disassembler.m680x.hcs08"_lang, CS_MODE_M680X_HCS08},
                                };

                                if (ImGui::BeginCombo("hex.builtin.view.disassembler.settings.mode"_lang, modes[selectedMode].first)) {
                                    for (u32 i = 0; i < IM_ARRAYSIZE(modes); i++) {
                                        if (ImGui::Selectable(modes[i].first))
                                            selectedMode = i;
                                    }
                                    ImGui::EndCombo();
                                }

                                this->m_mode = cs_mode(modes[selectedMode].second);
                            }
                            break;
                        case Architecture::MOS65XX:
                            {
                                static int selectedMode = 0;

                                std::pair<const char *, cs_mode> modes[] = {
                                    {"hex.builtin.view.disassembler.mos65xx.6502"_lang,           CS_MODE_MOS65XX_6502         },
                                    { "hex.builtin.view.disassembler.mos65xx.65c02"_lang,         CS_MODE_MOS65XX_65C02        },
                                    { "hex.builtin.view.disassembler.mos65xx.w65c02"_lang,        CS_MODE_MOS65XX_W65C02       },
                                    { "hex.builtin.view.disassembler.mos65xx.65816"_lang,         CS_MODE_MOS65XX_65816        },
                                    { "hex.builtin.view.disassembler.mos65xx.65816_long_m"_lang,  CS_MODE_MOS65XX_65816_LONG_M },
                                    { "hex.builtin.view.disassembler.mos65xx.65816_long_x"_lang,  CS_MODE_MOS65XX_65816_LONG_X },
                                    { "hex.builtin.view.disassembler.mos65xx.65816_long_mx"_lang, CS_MODE_MOS65XX_65816_LONG_MX},
                                };

                                if (ImGui::BeginCombo("hex.builtin.view.disassembler.settings.mode"_lang, modes[selectedMode].first)) {
                                    for (u32 i = 0; i < IM_ARRAYSIZE(modes); i++) {
                                        if (ImGui::Selectable(modes[i].first))
                                            selectedMode = i;
                                    }
                                    ImGui::EndCombo();
                                }

                                this->m_mode = cs_mode(modes[selectedMode].second);
                            }
                            break;
                        case Architecture::BPF:
                            {
                                static int mode = CS_MODE_BPF_CLASSIC;
                                ImGui::RadioButton("hex.builtin.view.disassembler.bpf.classic"_lang, &mode, CS_MODE_BPF_CLASSIC);
                                ImGui::SameLine();
                                ImGui::RadioButton("hex.builtin.view.disassembler.bpf.extended"_lang, &mode, CS_MODE_BPF_EXTENDED);

                                this->m_mode = cs_mode(mode);
                            }
                            break;
                        case Architecture::EVM:
                        case Architecture::TMS320C64X:
                        case Architecture::ARM64:
                        case Architecture::SYSZ:
                        case Architecture::XCORE:
                        case Architecture::WASM:
                        case Architecture::MAX:
                            this->m_mode = cs_mode(0);
                            break;
                    }
                }
                ImGui::EndChild();

                ImGui::BeginDisabled(this->m_disassemblerTask.isRunning());
                {
                    if (ImGui::Button("hex.builtin.view.disassembler.disassemble"_lang))
                        this->disassemble();
                }
                ImGui::EndDisabled();

                if (this->m_disassemblerTask.isRunning()) {
                    ImGui::SameLine();
                    ImGui::TextSpinner("hex.builtin.view.disassembler.disassembling"_lang);
                }

                ImGui::NewLine();

                ImGui::TextUnformatted("hex.builtin.view.disassembler.disassembly.title"_lang);
                ImGui::Separator();

                if (ImGui::BeginTable("##disassembly", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.builtin.view.disassembler.disassembly.address"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.disassembler.disassembly.offset"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.disassembler.disassembly.bytes"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.disassembler.disassembly.title"_lang);

                    if (!this->m_disassemblerTask.isRunning()) {
                        ImGuiListClipper clipper;
                        clipper.Begin(this->m_disassembly.size());

                        ImGui::TableHeadersRow();
                        while (clipper.Step()) {
                            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                                const auto &instruction = this->m_disassembly[i];
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                if (ImGui::Selectable(("##DisassemblyLine"s + std::to_string(i)).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                                    ImHexApi::HexEditor::setSelection(instruction.offset, instruction.size);
                                }
                                ImGui::SameLine();
                                ImGui::TextFormatted("0x{0:X}", instruction.address);
                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("0x{0:X}", instruction.offset);
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted(instruction.bytes.c_str());
                                ImGui::TableNextColumn();
                                ImGui::TextFormattedColored(ImColor(0xFFD69C56), "{}", instruction.mnemonic);
                                ImGui::SameLine();
                                ImGui::TextUnformatted(instruction.operators.c_str());
                            }
                        }

                        clipper.End();
                    }

                    ImGui::EndTable();
                }
            }
        }
        ImGui::End();
    }

}