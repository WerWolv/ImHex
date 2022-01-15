#include "content/views/view_disassembler.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>

#include <cstring>
#include <thread>

using namespace std::literals::string_literals;

namespace hex::plugin::builtin {

    ViewDisassembler::ViewDisassembler() : View("hex.builtin.view.disassembler.name") {
        EventManager::subscribe<EventDataChanged>(this, [this]() {
            this->disassemble();
        });

        EventManager::subscribe<EventRegionSelected>(this, [this](Region region) {
            if (this->m_shouldMatchSelection) {
                if (region.address == size_t(-1)) {
                    this->m_codeRegion[0] = this->m_codeRegion[1] = 0;
                } else {
                    this->m_codeRegion[0] = region.address;
                    this->m_codeRegion[1] = region.address + region.size;
                }
            }
        });

        EventManager::subscribe<EventFileUnloaded>(this, [this]{
            this->m_disassembly.clear();
        });
    }

    ViewDisassembler::~ViewDisassembler() {
        EventManager::unsubscribe<EventDataChanged>(this);
        EventManager::unsubscribe<EventRegionSelected>(this);
        EventManager::unsubscribe<EventFileUnloaded>(this);
    }

    void ViewDisassembler::disassemble() {
        this->m_disassembly.clear();
        this->m_disassembling = true;

        std::thread([this] {
            csh capstoneHandle;
            cs_insn *instructions = nullptr;

            cs_mode mode = cs_mode(this->m_modeBasicARM | this->m_modeExtraARM | this->m_modeBasicMIPS | this->m_modeBasicX86 | this->m_modeBasicPPC);

            if (this->m_littleEndianMode)
                mode = cs_mode(mode | CS_MODE_LITTLE_ENDIAN);
            else
                mode = cs_mode(mode | CS_MODE_BIG_ENDIAN);

            if (this->m_micoMode)
                mode = cs_mode(mode | CS_MODE_MICRO);

            if (this->m_sparcV9Mode)
                mode = cs_mode(mode | CS_MODE_V9);

            if (cs_open(Disassembler::toCapstoneArchictecture(this->m_architecture), mode, &capstoneHandle) == CS_ERR_OK) {

                cs_option(capstoneHandle, CS_OPT_SKIPDATA, CS_OPT_ON);

                auto provider = ImHexApi::Provider::get();
                std::vector<u8> buffer(2048, 0x00);
                size_t size = (this->m_codeRegion[1] - this->m_codeRegion[0] + 1);

                auto task = ImHexApi::Tasks::createTask("hex.builtin.view.disassembler.disassembling", size);
                for (u64 address = 0; address < size; address += 2048) {
                    task.update(address);

                    size_t bufferSize = std::min(u64(2048), (this->m_codeRegion[1] - this->m_codeRegion[0] + 1) - address);
                    provider->read(this->m_codeRegion[0] + address, buffer.data(), bufferSize);

                    size_t instructionCount = cs_disasm(capstoneHandle, buffer.data(), bufferSize, this->m_baseAddress + address, 0, &instructions);

                    if (instructionCount == 0)
                        break;

                    u64 usedBytes = 0;
                    for (u32 instr = 0; instr < instructionCount; instr++) {
                        Disassembly disassembly = { 0 };
                        disassembly.address = instructions[instr].address;
                        disassembly.offset = this->m_codeRegion[0] + address + usedBytes;
                        disassembly.size = instructions[instr].size;
                        disassembly.mnemonic = instructions[instr].mnemonic;
                        disassembly.operators = instructions[instr].op_str;

                        for (u16 i = 0; i < instructions[instr].size; i++)
                            disassembly.bytes += hex::format("{0:02X} ", instructions[instr].bytes[i]);
                        disassembly.bytes.pop_back();

                        this->m_disassembly.push_back(disassembly);

                        usedBytes += instructions[instr].size;
                    }

                    if (instructionCount < bufferSize)
                        address -= (bufferSize - usedBytes);

                    cs_free(instructions, instructionCount);
                }

                cs_close(&capstoneHandle);
            }

            this->m_disassembling = false;
        }).detach();

    }

    void ViewDisassembler::drawContent() {

        if (ImGui::Begin(View::toWindowName("hex.builtin.view.disassembler.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            auto provider = ImHexApi::Provider::get();
            if (ImHexApi::Provider::isValid() && provider->isReadable()) {
                ImGui::TextUnformatted("hex.builtin.view.disassembler.position"_lang);
                ImGui::Separator();

                ImGui::InputScalar("hex.builtin.view.disassembler.base"_lang, ImGuiDataType_U64, &this->m_baseAddress, nullptr, nullptr, "%08llX", ImGuiInputTextFlags_CharsHexadecimal);
                ImGui::InputScalarN("hex.builtin.view.disassembler.region"_lang, ImGuiDataType_U64, this->m_codeRegion, 2, nullptr, nullptr, "%08llX", ImGuiInputTextFlags_CharsHexadecimal);

                ImGui::Checkbox("hex.common.match_selection"_lang, &this->m_shouldMatchSelection);
                if (ImGui::IsItemEdited()) {
                    // Force execution of Region Selection Event
                    EventManager::post<RequestSelectionChange>(Region{ 0, 0 });
                }

                ImGui::NewLine();
                ImGui::TextUnformatted("hex.builtin.view.disassembler.settings.header"_lang);
                ImGui::Separator();

                ImGui::Combo("hex.builtin.view.disassembler.arch"_lang, reinterpret_cast<int*>(&this->m_architecture), Disassembler::ArchitectureNames, Disassembler::getArchitectureSupportedCount());


                if (ImGui::BeginChild("modes", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 6), true, ImGuiWindowFlags_AlwaysAutoResize)) {

                    if (ImGui::RadioButton("hex.common.little_endian"_lang, this->m_littleEndianMode))
                        this->m_littleEndianMode = true;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("hex.common.big_endian"_lang, !this->m_littleEndianMode))
                        this->m_littleEndianMode = false;

                    ImGui::NewLine();

                    switch (this->m_architecture) {
                        case Architecture::ARM:
                            this->m_modeBasicMIPS = cs_mode(0);
                            this->m_modeBasicX86 = cs_mode(0);
                            this->m_modeBasicPPC = cs_mode(0);
                            this->m_micoMode = false;
                            this->m_sparcV9Mode = false;

                            if (this->m_modeBasicARM == cs_mode(0))
                                this->m_modeBasicARM = CS_MODE_ARM;

                            if (ImGui::RadioButton("hex.builtin.view.disassembler.arm.arm"_lang, this->m_modeBasicARM == CS_MODE_ARM))
                                this->m_modeBasicARM = CS_MODE_ARM;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("hex.builtin.view.disassembler.arm.thumb"_lang, this->m_modeBasicARM == CS_MODE_THUMB))
                                this->m_modeBasicARM = CS_MODE_THUMB;

                            if (ImGui::RadioButton("hex.builtin.view.disassembler.arm.default"_lang, (this->m_modeExtraARM & (CS_MODE_MCLASS | CS_MODE_V8)) == 0))
                                this->m_modeExtraARM = cs_mode(0);
                            ImGui::SameLine();
                            if (ImGui::RadioButton("hex.builtin.view.disassembler.arm.cortex_m"_lang, (this->m_modeExtraARM & (CS_MODE_MCLASS | CS_MODE_V8)) == CS_MODE_MCLASS))
                                this->m_modeExtraARM = CS_MODE_MCLASS;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("hex.builtin.view.disassembler.arm.armv8"_lang, (this->m_modeExtraARM & (CS_MODE_MCLASS | CS_MODE_V8)) == CS_MODE_V8))
                                this->m_modeExtraARM = CS_MODE_V8;
                            break;
                        case Architecture::MIPS:
                            this->m_modeBasicARM = cs_mode(0);
                            this->m_modeExtraARM = cs_mode(0);
                            this->m_modeBasicX86 = cs_mode(0);
                            this->m_modeBasicPPC = cs_mode(0);
                            this->m_sparcV9Mode = false;

                            if (this->m_modeBasicMIPS == cs_mode(0))
                                this->m_modeBasicMIPS = CS_MODE_MIPS32;

                            if (ImGui::RadioButton("hex.builtin.view.disassembler.mips.mips32"_lang, this->m_modeBasicMIPS == CS_MODE_MIPS32))
                                this->m_modeBasicMIPS = CS_MODE_MIPS32;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("hex.builtin.view.disassembler.mips.mips64"_lang, this->m_modeBasicMIPS == CS_MODE_MIPS64))
                                this->m_modeBasicMIPS = CS_MODE_MIPS64;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("hex.builtin.view.disassembler.mips.mips32R6"_lang, this->m_modeBasicMIPS == CS_MODE_MIPS32R6))
                                this->m_modeBasicMIPS = CS_MODE_MIPS32R6;

                            ImGui::Checkbox("hex.builtin.view.disassembler.mips.micro"_lang, &this->m_micoMode);
                            break;
                        case Architecture::X86:
                            this->m_modeBasicARM = cs_mode(0);
                            this->m_modeExtraARM = cs_mode(0);
                            this->m_modeBasicMIPS = cs_mode(0);
                            this->m_modeBasicPPC = cs_mode(0);
                            this->m_micoMode = false;
                            this->m_sparcV9Mode = false;

                            if (this->m_modeBasicX86 == cs_mode(0))
                                this->m_modeBasicX86 = CS_MODE_16;

                            if (ImGui::RadioButton("hex.builtin.view.disassembler.x86.16bit"_lang, this->m_modeBasicX86 == CS_MODE_16))
                                this->m_modeBasicX86 = CS_MODE_16;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("hex.builtin.view.disassembler.x86.32bit"_lang, this->m_modeBasicX86 == CS_MODE_32))
                                this->m_modeBasicX86 = CS_MODE_32;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("hex.builtin.view.disassembler.x86.64bit"_lang, this->m_modeBasicX86 == CS_MODE_64))
                                this->m_modeBasicX86 = CS_MODE_64;
                            break;
                        case Architecture::PPC:
                            this->m_modeBasicARM = cs_mode(0);
                            this->m_modeExtraARM = cs_mode(0);
                            this->m_modeBasicMIPS = cs_mode(0);
                            this->m_modeBasicX86 = cs_mode(0);
                            this->m_micoMode = false;
                            this->m_sparcV9Mode = false;

                            if (m_modeBasicPPC == cs_mode(0))
                                this->m_modeBasicPPC = CS_MODE_32;

                            if (ImGui::RadioButton("hex.builtin.view.disassembler.ppc.32bit"_lang, this->m_modeBasicPPC == CS_MODE_32))
                                this->m_modeBasicPPC = CS_MODE_32;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("hex.builtin.view.disassembler.ppc.64bit"_lang, this->m_modeBasicPPC == CS_MODE_64))
                                this->m_modeBasicPPC = CS_MODE_64;
                            break;
                        case Architecture::SPARC:
                            this->m_modeBasicARM = cs_mode(0);
                            this->m_modeExtraARM = cs_mode(0);
                            this->m_modeBasicMIPS = cs_mode(0);
                            this->m_modeBasicX86 = cs_mode(0);
                            this->m_modeBasicPPC = cs_mode(0);
                            this->m_micoMode = false;

                            ImGui::Checkbox("hex.builtin.view.disassembler.sparc.v9"_lang, &this->m_sparcV9Mode);
                            break;
                        case Architecture::ARM64:
                        case Architecture::SYSZ:
                        case Architecture::XCORE:
                        case Architecture::M68K:
                        case Architecture::TMS320C64X:
                        case Architecture::M680X:
                        case Architecture::EVM:
                            this->m_modeBasicARM = cs_mode(0);
                            this->m_modeExtraARM = cs_mode(0);
                            this->m_modeBasicMIPS = cs_mode(0);
                            this->m_modeBasicX86 = cs_mode(0);
                            this->m_modeBasicPPC = cs_mode(0);
                            this->m_micoMode = false;
                            this->m_sparcV9Mode = false;
                            break;
                    }

                }
                ImGui::EndChild();

                ImGui::Disabled([this] {
                    if (ImGui::Button("hex.builtin.view.disassembler.disassemble"_lang))
                        this->disassemble();
                }, this->m_disassembling);

                if (this->m_disassembling) {
                    ImGui::SameLine();
                    ImGui::TextSpinner("hex.builtin.view.disassembler.disassembling"_lang);
                }

                ImGui::NewLine();

                ImGui::TextUnformatted("hex.builtin.view.disassembler.disassembly.title"_lang);
                ImGui::Separator();

                if (ImGui::BeginTable("##disassembly", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Reorderable)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.builtin.view.disassembler.disassembly.address"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.disassembler.disassembly.offset"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.disassembler.disassembly.bytes"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.disassembler.disassembly.title"_lang);

                    if (!this->m_disassembling) {
                        ImGuiListClipper clipper;
                        clipper.Begin(this->m_disassembly.size());

                        ImGui::TableHeadersRow();
                        while (clipper.Step()) {
                            for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                                const auto &instruction = this->m_disassembly[i];
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                if (ImGui::Selectable(("##DisassemblyLine"s + std::to_string(i)).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                                    EventManager::post<RequestSelectionChange>(Region { instruction.offset, instruction.size });
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

    void ViewDisassembler::drawMenu() {

    }

}