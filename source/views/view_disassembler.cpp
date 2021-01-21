#include "views/view_disassembler.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>

#include <cstring>

using namespace std::literals::string_literals;

namespace hex {

    ViewDisassembler::ViewDisassembler() : View("Disassembler") {
        View::subscribeEvent(Events::DataChanged, [this](auto){
            this->m_shouldInvalidate = true;
        });

        View::subscribeEvent(Events::RegionSelected, [this](auto userData) {
            auto region = std::any_cast<Region>(userData);

            if (this->m_shouldMatchSelection) {
                this->m_codeRegion[0] = region.address;
                this->m_codeRegion[1] = region.address + region.size - 1;
            }
        });
    }

    ViewDisassembler::~ViewDisassembler() {
        View::unsubscribeEvent(Events::DataChanged);
        View::unsubscribeEvent(Events::RegionSelected);
    }

    void ViewDisassembler::drawContent() {
        if (this->m_shouldInvalidate) {
            this->m_disassembly.clear();

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

                auto provider = SharedData::currentProvider;
                std::vector<u8> buffer(2048, 0x00);
                for (u64 address = 0; address < (this->m_codeRegion[1] - this->m_codeRegion[0] + 1); address += 2048) {
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

                        for (u8 i = 0; i < instructions[instr].size; i++)
                            disassembly.bytes += hex::format("%02X ", instructions[instr].bytes[i]);
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

            this->m_shouldInvalidate = false;
        }


        if (ImGui::Begin("Disassembler", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            auto provider = SharedData::currentProvider;
            if (provider != nullptr && provider->isReadable()) {
                ImGui::TextUnformatted("Position");
                ImGui::Separator();

                ImGui::InputScalar("Base address", ImGuiDataType_U64, &this->m_baseAddress, nullptr, nullptr, "%08llX", ImGuiInputTextFlags_CharsHexadecimal);
                ImGui::InputScalarN("Code region", ImGuiDataType_U64, this->m_codeRegion, 2, nullptr, nullptr, "%08llX", ImGuiInputTextFlags_CharsHexadecimal);
                ImGui::Checkbox("Match selection", &this->m_shouldMatchSelection);

                ImGui::NewLine();
                ImGui::TextUnformatted("Settings");
                ImGui::Separator();

                ImGui::Combo("Architecture", reinterpret_cast<int*>(&this->m_architecture), Disassembler::ArchitectureNames, Disassembler::getArchitectureSupportedCount());


                if (ImGui::BeginChild("modes", ImVec2(0, 100), true)) {

                    if (ImGui::RadioButton("Little Endian", this->m_littleEndianMode))
                        this->m_littleEndianMode = true;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Big Endian", !this->m_littleEndianMode))
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

                            if (ImGui::RadioButton("ARM mode", this->m_modeBasicARM == CS_MODE_ARM))
                                this->m_modeBasicARM = CS_MODE_ARM;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("Thumb mode", this->m_modeBasicARM == CS_MODE_THUMB))
                                this->m_modeBasicARM = CS_MODE_THUMB;

                            if (ImGui::RadioButton("Default mode", (this->m_modeExtraARM & (CS_MODE_MCLASS | CS_MODE_V8)) == 0))
                                this->m_modeExtraARM = cs_mode(0);
                            ImGui::SameLine();
                            if (ImGui::RadioButton("Cortex-M mode", (this->m_modeExtraARM & (CS_MODE_MCLASS | CS_MODE_V8)) == CS_MODE_MCLASS))
                                this->m_modeExtraARM = CS_MODE_MCLASS;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("ARMv8 mode", (this->m_modeExtraARM & (CS_MODE_MCLASS | CS_MODE_V8)) == CS_MODE_V8))
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

                            if (ImGui::RadioButton("MIPS32 mode", this->m_modeBasicMIPS == CS_MODE_MIPS32))
                                this->m_modeBasicMIPS = CS_MODE_MIPS32;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("MIPS64 mode", this->m_modeBasicMIPS == CS_MODE_MIPS64))
                                this->m_modeBasicMIPS = CS_MODE_MIPS64;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("MIPS32R6 mode", this->m_modeBasicMIPS == CS_MODE_MIPS32R6))
                                this->m_modeBasicMIPS = CS_MODE_MIPS32R6;

                            ImGui::Checkbox("Micro Mode", &this->m_micoMode);
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

                            if (ImGui::RadioButton("16-bit mode", this->m_modeBasicX86 == CS_MODE_16))
                                this->m_modeBasicX86 = CS_MODE_16;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("32-bit mode", this->m_modeBasicX86 == CS_MODE_32))
                                this->m_modeBasicX86 = CS_MODE_32;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("64-bit mode", this->m_modeBasicX86 == CS_MODE_64))
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

                            if (ImGui::RadioButton("32-bit mode", this->m_modeBasicPPC == CS_MODE_32))
                                this->m_modeBasicPPC = CS_MODE_32;
                            ImGui::SameLine();
                            if (ImGui::RadioButton("64-bit mode", this->m_modeBasicPPC == CS_MODE_64))
                                this->m_modeBasicPPC = CS_MODE_64;
                            break;
                        case Architecture::SPARC:
                            this->m_modeBasicARM = cs_mode(0);
                            this->m_modeExtraARM = cs_mode(0);
                            this->m_modeBasicMIPS = cs_mode(0);
                            this->m_modeBasicX86 = cs_mode(0);
                            this->m_modeBasicPPC = cs_mode(0);
                            this->m_micoMode = false;

                            ImGui::Checkbox("Sparc V9 mode", &this->m_sparcV9Mode);
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

                ImGui::SetCursorPosX((ImGui::GetContentRegionAvailWidth() - 300) / 2);
                if (ImGui::Button("Disassemble", ImVec2(300, 20)))
                    this->m_shouldInvalidate = true;
                ImGui::NewLine();

                ImGui::TextUnformatted("Disassembly");
                ImGui::Separator();

                if (ImGui::BeginTable("##disassembly", 4, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Reorderable)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Address");
                    ImGui::TableSetupColumn("Offset");
                    ImGui::TableSetupColumn("Bytes");
                    ImGui::TableSetupColumn("Disassembly");

                    ImGuiListClipper clipper;
                    clipper.Begin(this->m_disassembly.size());

                    ImGui::TableHeadersRow();
                    while (clipper.Step()) {
                        for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            if (ImGui::Selectable(("##DisassemblyLine"s + std::to_string(i)).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                                Region selectRegion = { this->m_disassembly[i].offset, this->m_disassembly[i].size };
                                View::postEvent(Events::SelectionChangeRequest, selectRegion);
                            }
                            ImGui::SameLine();
                            ImGui::Text("0x%llx", this->m_disassembly[i].address);
                            ImGui::TableNextColumn();
                            ImGui::Text("0x%llx", this->m_disassembly[i].offset);
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(this->m_disassembly[i].bytes.c_str());
                            ImGui::TableNextColumn();
                            ImGui::TextColored(ImColor(0xFFD69C56), "%s", this->m_disassembly[i].mnemonic.c_str());
                            ImGui::SameLine();
                            ImGui::TextUnformatted(this->m_disassembly[i].operators.c_str());
                        }
                    }

                    clipper.End();

                    ImGui::EndTable();
                }
            }
        }
        ImGui::End();
    }

    void ViewDisassembler::drawMenu() {

    }

}