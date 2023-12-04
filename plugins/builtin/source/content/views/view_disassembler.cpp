#include "content/views/view_disassembler.hpp"

#include <hex/helpers/fmt.hpp>

#include <hex/providers/buffered_reader.hpp>

using namespace std::literals::string_literals;

namespace hex::plugin::builtin {

    ViewDisassembler::ViewDisassembler() : View::Window("hex.builtin.view.disassembler.name") {

    }

    void ViewDisassembler::addLine(prv::Provider *provider, const ContentRegistry::Disassembler::Instruction &instruction) {
        prv::ProviderReader reader(provider);
        reader.seek(instruction.region.getStartAddress());
        reader.setEndAddress(instruction.region.getEndAddress());

        std::string bytes;
        for (const auto& byte : reader) {
            bytes += fmt::format("{:02X} ", byte);
        }
        bytes.pop_back();

        this->m_lines.get(provider).emplace_back(ImHexApi::HexEditor::ProviderRegion { instruction.region, provider }, std::move(bytes), instruction.mnemonic, instruction.operands, instruction.jumpDestination, ImVec2());
    }

    static void drawJumpLine(ImVec2 start, ImVec2 end, float columnWidth, u32 slot, bool endVisible, bool hovered) {
        const u32 slotCount = std::floor(std::max<float>(1.0F, columnWidth / 10_scaled));

        if (slot >= slotCount)
            return;

        auto drawList = ImGui::GetWindowDrawList();

        if (start.y > end.y)
            slot = slotCount - slot - 1;

        const auto width = (columnWidth / float(slotCount)) * float(slot + 1);
        const auto lineColor = ImColor::HSV(hovered ? 0.25F : 0.3F + (slot / float(slotCount)) * 0.7F, hovered ? 1.0F : 0.8F, hovered ? 1.0F : 0.8F);
        const auto thickness = 2.0_scaled;

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

        if (this->m_lines->empty()) {
            auto provider = ImHexApi::Provider::get();
            if (ImGuiExt::DimmedButton("Disassemble", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                auto disassembly = this->m_currArchitecture->disassemble(provider, ImHexApi::HexEditor::getSelection().value());

                for (const auto &instruction : disassembly)
                    this->addLine(provider, instruction);
            }

            ImGuiExt::BeginSubWindow("Config");
            {
                this->m_currArchitecture->drawConfigInterface();
            }
            ImGuiExt::EndSubWindow();
        } else {
            if (ImGuiExt::DimmedButton("Reset", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                this->m_lines->clear();
            }
        }

        if (ImGui::BeginTable("##disassembly", 4, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable, ImGui::GetContentRegionAvail())) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("##jumps");
            ImGui::TableSetupColumn("##address", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 80_scaled);
            ImGui::TableSetupColumn("##bytes", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 80_scaled);
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

                    auto height = ImGui::CalcTextSize(line.bytes.c_str(), nullptr, false, 80_scaled).y;

                    ImGui::TableNextColumn();
                    {
                        // Reserve some space to draw the jump lines later

                        // Remember the position of the line so we can draw the jump lines later
                        jumpColumnWidth = ImGui::GetContentRegionAvail().x;
                        line.linePos = ImGui::GetCursorScreenPos() + ImVec2(jumpColumnWidth, height / 2);
                    }

                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(hex::format("0x{:08X}", line.region.getStartAddress()).c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, height))) {
                        ImHexApi::HexEditor::setSelection(line.region);
                    }

                    if (ImGui::IsItemHovered())
                        hoveredAddress = line.region.getStartAddress();

                    ImGui::TableNextColumn();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_Highlight));
                    ImGuiExt::TextFormattedWrapped("{}", line.bytes);
                    ImGui::PopStyleColor();

                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormattedColored(ImGui::GetColorU32(ImGuiCol_HeaderActive), "{} ", line.mnemonic);
                    ImGui::SameLine(0, 0);
                    ImGuiExt::TextFormatted("{}", line.operands);
                }
            }

            // Draw jump arrows
            if (!this->m_lines->empty()) {
                auto &firstVisibleLine = this->m_lines->at(processingStart);
                auto &lastVisibleLine = this->m_lines->at(processingEnd - 1);

                std::list<u64> destinations;
                for (auto sourceLineIndex = processingStart; sourceLineIndex < processingEnd; sourceLineIndex += 1) {
                    const auto &sourceLine = this->m_lines->at(sourceLineIndex);

                    if (auto jumpDestination = sourceLine.jumpDestination; jumpDestination.has_value()) {
                        for (auto destinationLineIndex = processingStart; destinationLineIndex < processingEnd; destinationLineIndex += 1) {
                            const auto &destinationLine = this->m_lines->at(destinationLineIndex);

                            if (*jumpDestination == destinationLine.region.getStartAddress()) {
                                drawJumpLine(sourceLine.linePos, destinationLine.linePos, jumpColumnWidth, destinations.size(), true, hoveredAddress == sourceLine.region.getStartAddress() || hoveredAddress == destinationLine.region.getStartAddress());
                                destinations.push_back(*jumpDestination);
                                break;
                            } else if (*jumpDestination > lastVisibleLine.region.getStartAddress()) {
                                drawJumpLine(sourceLine.linePos, lastVisibleLine.linePos, jumpColumnWidth, destinations.size(), false, hoveredAddress == sourceLine.region.getStartAddress() || hoveredAddress == destinationLine.region.getStartAddress());
                                destinations.push_back(*jumpDestination);
                                break;
                            } else if (*jumpDestination < firstVisibleLine.region.getStartAddress()) {
                                drawJumpLine(sourceLine.linePos, firstVisibleLine.linePos, jumpColumnWidth, destinations.size(), false, hoveredAddress == sourceLine.region.getStartAddress() || hoveredAddress == destinationLine.region.getStartAddress());
                                destinations.push_back(*jumpDestination);
                                break;
                            }
                        }
                    }

                    std::erase_if(destinations, [&](u64 address) {
                        return sourceLine.region.getStartAddress() == address;
                    });
                }
            }

            ImGui::EndTable();
        }
    }

}
