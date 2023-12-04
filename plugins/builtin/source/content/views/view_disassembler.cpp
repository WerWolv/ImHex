#include "content/views/view_disassembler.hpp"

#include <hex/helpers/fmt.hpp>

#include <hex/providers/buffered_reader.hpp>
#include <ui/widgets.hpp>

using namespace std::literals::string_literals;

namespace hex::plugin::builtin {

    ViewDisassembler::ViewDisassembler() : View::Window("hex.builtin.view.disassembler.name") {

    }

    void ViewDisassembler::addLine(prv::Provider *provider, const ContentRegistry::Disassembler::Instruction &instruction) {
        std::vector<u8> bytes(instruction.region.getSize());
        provider->read(instruction.region.getStartAddress(), bytes.data(), bytes.size());

        std::string byteString;
        for (const auto& byte : bytes) {
            byteString += fmt::format("{:02X} ", byte);
        }
        byteString.pop_back();

        switch (instruction.type) {
            using enum ContentRegistry::Disassembler::Instruction::Type;
            case Return:
                this->m_lines.get(provider).emplace_back(
                        DisassemblyLine::Type::Instruction,
                        ImHexApi::HexEditor::ProviderRegion {
                            instruction.region,
                            provider
                        },
                        std::move(byteString),
                        instruction.mnemonic,
                        instruction.operands,
                        instruction.extraData,
                        ImVec2()
                    );
                this->m_lines.get(provider).emplace_back(DisassemblyLine::Type::Separator);
                break;
            case Call:
                this->m_lines.get(provider).emplace_back(
                        DisassemblyLine::Type::CallInstruction,
                        ImHexApi::HexEditor::ProviderRegion {
                            instruction.region,
                            provider
                        },
                        std::move(byteString),
                        instruction.mnemonic,
                        instruction.operands,
                        instruction.extraData,
                        ImVec2()
                    );
                break;
            case Jump:
            case Other:
                this->m_lines.get(provider).emplace_back(
                        DisassemblyLine::Type::Instruction,
                        ImHexApi::HexEditor::ProviderRegion {
                            instruction.region,
                            provider
                        },
                        std::move(byteString),
                        instruction.mnemonic,
                        instruction.operands,
                        instruction.extraData,
                        ImVec2()
                    );
                break;
        }
    }

    bool ViewDisassembler::drawInstructionLine(DisassemblyLine& line) {
        auto height = ImGui::GetTextLineHeight(); //ImGui::CalcTextSize(line.bytes.c_str(), nullptr, false, 80_scaled).y;

        ImGui::TableNextColumn();
        if (ImGui::Selectable(hex::format("0x{:08X}", line.region.getStartAddress()).c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, height))) {
            ImHexApi::HexEditor::setSelection(line.region);
        }

        bool hovered = ImGui::IsItemHovered();

        ImGui::TableNextColumn();
        ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_Highlight), "{}", line.bytes);

        ImGui::TableNextColumn();
        ImGuiExt::TextFormattedColored(ImGui::GetColorU32(ImGuiCol_HeaderActive), "{} ", line.mnemonic);
        ImGui::SameLine(0, 0);
        ImGuiExt::TextFormatted("{}", line.operands);

        return hovered;
    }

    bool ViewDisassembler::drawSeparatorLine(DisassemblyLine&) {
        ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetColorU32(ImGuiCol_Text));
        ImGui::Selectable("##separator", true, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 2_scaled));
        ImGui::PopStyleColor();
        ImGui::EndDisabled();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();

        return false;
    }



    static void drawJumpLine(ImVec2 start, ImVec2 end, float columnWidth, u32 slot, bool endVisible, bool hovered) {
        const u32 slotCount = std::floor(std::max<float>(1.0F, columnWidth / 10_scaled));

        if (slot >= slotCount)
            return;

        auto drawList = ImGui::GetWindowDrawList();

        const auto width = (columnWidth / float(slotCount)) * float(slot + 1);
        const auto lineColor = ImColor::HSV(hovered ? 0.25F : 0.3F + (slot / float(slotCount)) * 0.7F, hovered ? 1.0F : 0.8F, hovered ? 1.0F : 0.8F);
        const auto thickness = 2.0_scaled;

        // Handle jump to same address
        if (start.y == end.y) {
            start.y -= ImGui::GetTextLineHeight() / 2;
            end.y   += ImGui::GetTextLineHeight() / 2;
        } else if (start.y > end.y) {
            slot = slotCount - (slot - 1);
        }


        // Draw vertical arrow line
        drawList->AddLine(start - ImVec2(width, 0), end - ImVec2(width, 0), lineColor, thickness);

        // Draw horizontal arrow line at start
        drawList->AddLine(start - ImVec2(width, 0), start, lineColor, thickness);

        if (endVisible) {
            // Draw horizontal arrow line at end
            drawList->AddLine(end   - ImVec2(width, 0), end,   lineColor, thickness);

            // Draw arrow head
            drawList->AddLine(end + scaled({ -5, -5 }), end, lineColor, thickness);
            drawList->AddLine(end + scaled({ -5,  5 }), end, lineColor, thickness);
        }
    }


    void ViewDisassembler::drawContent() {
        const auto &architectures = ContentRegistry::Disassembler::impl::getArchitectures();
        if (this->m_currArchitecture == nullptr) {
            this->m_currArchitecture = architectures.front().get();
        }

        ImGui::BeginDisabled(!this->m_lines->empty());
        if (ImGui::BeginCombo("##architectures", Lang(this->m_currArchitecture->getUnlocalizedName()))) {
            for (const auto &architecture : architectures) {
                if (ImGui::Selectable(Lang(architecture->getUnlocalizedName()))) {
                    this->m_currArchitecture = architecture.get();
                }
            }

            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        if (this->m_disassembleTask.isRunning() || this->m_lines->empty()) {
            ImGui::BeginDisabled(this->m_disassembleTask.isRunning());

            auto provider = ImHexApi::Provider::get();
            if (ImGuiExt::DimmedButton("Disassemble", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                this->m_disassembleTask = TaskManager::createTask("Disassembling...", this->m_disassembleRegion.getSize(), [this, provider](auto &task) {
                    const auto disassembly = this->m_currArchitecture->disassemble(provider, this->m_disassembleRegion, task);

                    task.setMaxValue(disassembly.size());
                    for (const auto &[index, instruction] : disassembly | std::views::enumerate) {
                        task.update(index);
                        this->addLine(provider, instruction);
                    }
                });
            }

            ImGuiExt::BeginSubWindow("Config");
            {
                ui::regionSelectionPicker(&this->m_disassembleRegion, provider, &this->m_regionType, true, true);

                ImGuiExt::Header("Architecture Settings");
                this->m_currArchitecture->drawConfigInterface();
            }
            ImGuiExt::EndSubWindow();

            ImGui::EndDisabled();
        } else {
            if (ImGuiExt::DimmedButton("Reset", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                this->m_lines->clear();
            }

            if (ImGui::BeginTable("##disassembly", 4, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable, ImGui::GetContentRegionAvail())) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("##jumps");
                ImGui::TableSetupColumn("##address", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 80_scaled);
                ImGui::TableSetupColumn("##bytes", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 120_scaled);
                ImGui::TableSetupColumn("##instruction", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize);

                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(this->m_lines->size(), ImGui::GetTextLineHeightWithSpacing());

                int processingStart = 0, processingEnd = 0;

                float jumpColumnWidth = 0.0F;
                std::optional<u64> hoveredAddress;
                while (clipper.Step()) {
                    processingStart = clipper.DisplayStart;
                    processingEnd = clipper.DisplayEnd;
                    for (auto i = processingStart; i < processingEnd; i += 1) {
                        auto &line = this->m_lines->at(i);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        {
                            auto height = ImGui::CalcTextSize(line.bytes.c_str(), nullptr, false, 80_scaled).y;
                            // Reserve some space to draw the jump lines later

                            // Remember the position of the line so we can draw the jump lines later
                            jumpColumnWidth = ImGui::GetContentRegionAvail().x;
                            line.linePosition = ImGui::GetCursorScreenPos() + ImVec2(jumpColumnWidth, height / 2);
                        }

                        switch (line.type) {
                            using enum DisassemblyLine::Type;
                            case CallInstruction:
                            case Instruction:
                                if (this->drawInstructionLine(line))
                                    hoveredAddress = line.region.getStartAddress();
                                break;
                            case Separator:
                                this->drawSeparatorLine(line);
                                break;
                        }

                    }
                }

                ImGui::EndTable();

                // Draw jump arrows
                if (!this->m_lines->empty()) {
                    auto &firstVisibleLine = this->m_lines->at(processingStart);
                    auto &lastVisibleLine = this->m_lines->at(processingEnd - 1);

                    const u32 slotCount = std::floor(std::max<float>(1.0F, jumpColumnWidth / 10_scaled));
                    std::map<u64, u64> occupiedSlots;

                    auto findFreeSlot = [&](u64 jumpDestination) {
                        for (u32 i = 0; i < slotCount; i += 1) {
                            if (!occupiedSlots.contains(i) || occupiedSlots[i] == jumpDestination) {
                                return i;
                            }
                        }

                        return slotCount;
                    };

                    for (auto sourceLineIndex = processingStart; sourceLineIndex < processingEnd; sourceLineIndex += 1) {
                        const auto &sourceLine = this->m_lines->at(sourceLineIndex);

                        if (auto jumpDestination = sourceLine.extraData; jumpDestination.has_value()) {
                            for (auto destinationLineIndex = processingStart; destinationLineIndex < processingEnd; destinationLineIndex += 1) {
                                const auto &destinationLine = this->m_lines->at(destinationLineIndex);

                                const auto freeSlot = findFreeSlot(*jumpDestination);
                                const bool hovered = hoveredAddress == sourceLine.region.getStartAddress() ||
                                                     hoveredAddress == destinationLine.region.getStartAddress();

                                bool jumpFound = false;
                                if (*jumpDestination == destinationLine.region.getStartAddress()) {
                                    drawJumpLine(sourceLine.linePosition, destinationLine.linePosition, jumpColumnWidth, freeSlot, true, hovered);
                                    jumpFound = true;
                                } else if (*jumpDestination > lastVisibleLine.region.getStartAddress()) {
                                    drawJumpLine(sourceLine.linePosition, lastVisibleLine.linePosition, jumpColumnWidth, freeSlot, false, hovered);
                                    jumpFound = true;
                                } else if (*jumpDestination < firstVisibleLine.region.getStartAddress()) {
                                    drawJumpLine(sourceLine.linePosition, firstVisibleLine.linePosition, jumpColumnWidth, freeSlot, false, hovered);
                                    jumpFound = true;
                                }

                                if (jumpFound) {
                                    if (!occupiedSlots.contains(freeSlot))
                                        occupiedSlots[freeSlot] = *jumpDestination;
                                    break;
                                }
                            }
                        }

                        std::erase_if(occupiedSlots, [&](const auto &entry) {
                            auto &[slot, destination] = entry;
                            return sourceLine.extraData.value() == destination;
                        });
                    }
                }
            }
        }

    }

}
