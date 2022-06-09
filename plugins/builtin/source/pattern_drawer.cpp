#include "pattern_drawer.hpp"

#include <pl/patterns/pattern_array_dynamic.hpp>
#include <pl/patterns/pattern_array_static.hpp>
#include <pl/patterns/pattern_bitfield.hpp>
#include <pl/patterns/pattern_boolean.hpp>
#include <pl/patterns/pattern_character.hpp>
#include <pl/patterns/pattern_enum.hpp>
#include <pl/patterns/pattern_float.hpp>
#include <pl/patterns/pattern_padding.hpp>
#include <pl/patterns/pattern_pointer.hpp>
#include <pl/patterns/pattern_signed.hpp>
#include <pl/patterns/pattern_string.hpp>
#include <pl/patterns/pattern_struct.hpp>
#include <pl/patterns/pattern_union.hpp>
#include <pl/patterns/pattern_unsigned.hpp>
#include <pl/patterns/pattern_wide_character.hpp>
#include <pl/patterns/pattern_wide_string.hpp>

#include <string>

#include <hex/api/imhex_api.hpp>
#include <hex/helpers/utils.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace {
    constexpr static auto DisplayEndDefault = 50u;
    constexpr static auto DisplayEndStep = 50u;

    using namespace ::std::literals::string_literals;
};

namespace hex {

    void PatternDrawer::visit(pl::PatternArrayDynamic& pattern) {
        this->drawArray(pattern);
    }

    void PatternDrawer::visit(pl::PatternArrayStatic& pattern) {
        this->drawArray(pattern);
    }

    void PatternDrawer::visit(pl::PatternBitfieldField& pattern) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        drawNameColumn(pattern);
        makeSelectable(pattern);
        drawColorColumn(pattern);

        auto byteAddr = pattern.getOffset() + pattern.getBitOffset() / 8;
        auto firstBitIdx = pattern.getBitOffset() % 8;
        auto lastBitIdx = firstBitIdx + (pattern.getBitSize() - 1) % 8;
        if (firstBitIdx == lastBitIdx)
            ImGui::TextFormatted("0x{0:08X} bit {1}", byteAddr, firstBitIdx);
        else
            ImGui::TextFormatted("0x{0:08X} bits {1} - {2}", byteAddr, firstBitIdx, lastBitIdx);
        ImGui::TableNextColumn();
        if (pattern.getBitSize() == 1)
            ImGui::TextFormatted("{0} bit", pattern.getBitSize());
        else
            ImGui::TextFormatted("{0} bits", pattern.getBitSize());
        ImGui::TableNextColumn();
        ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "bits");
        ImGui::TableNextColumn();

        ImGui::TextFormatted("{}", pattern.getFormattedValue());
    }

    void PatternDrawer::visit(pl::PatternBitfield& pattern) {
        bool open = true;
        if (!pattern.isInlined()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            ImGui::TableNextColumn();
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            drawTypenameColumn(pattern, "bitfield");

            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        } else {
            ImGui::SameLine();
            ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf);
        }

        if (open) {
            pattern.forEachMember([&] (auto &field) {
                this->draw(field);
            });

            ImGui::TreePop();
        }
    }

    void PatternDrawer::visit(pl::PatternBoolean& pattern) {
        this->createDefaultEntry(pattern, pattern.getFormattedValue(), static_cast<bool>(pattern.getValue()));
    }

    void PatternDrawer::visit(pl::PatternCharacter& pattern) {
        this->createDefaultEntry(pattern, pattern.getFormattedValue(), pattern.getValue());
    }

    void PatternDrawer::visit(pl::PatternEnum& pattern) {
        ImGui::TableNextRow();
        createLeafNode(pattern);
        drawCommentTooltip(pattern);
        ImGui::TableNextColumn();
        makeSelectable(pattern);
        ImGui::SameLine();
        drawNameColumn(pattern);
        drawColorColumn(pattern);
        drawOffsetColumn(pattern);
        drawSizeColumn(pattern);
        drawTypenameColumn(pattern, "enum");
        ImGui::TextFormatted("{}", pattern.getFormattedValue());
    }

    void PatternDrawer::visit(pl::PatternFloat& pattern) {
        if (pattern.getSize() == 4) {
            this->createDefaultEntry(pattern, pattern.getFormattedValue(), static_cast<float>(pattern.getValue()));
        } else if (pattern.getSize() == 8) {
            this->createDefaultEntry(pattern, pattern.getFormattedValue(), static_cast<double>(pattern.getValue()));
        }
    }

    void PatternDrawer::visit(pl::PatternPadding& pattern) {
        // Do nothing
        hex::unused(pattern);
    }

    void PatternDrawer::visit(pl::PatternPointer& pattern) {
        bool open = true;

        if (!pattern.isInlined()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            ImGui::SameLine(0, 0);
            drawColorColumn(pattern);
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{}", pattern.getFormattedName());
            ImGui::TableNextColumn();
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        } else {
            ImGui::SameLine();
            ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf);
        }

        if (open) {
            pattern.getPointedAtPattern()->accept(*this);

            ImGui::TreePop();
        }
    }

    void PatternDrawer::visit(pl::PatternSigned& pattern) {
        this->createDefaultEntry(pattern, pattern.getFormattedValue(), pattern.getValue());
    }

    void PatternDrawer::visit(pl::PatternString& pattern) {
        this->createDefaultEntry(pattern, pattern.getFormattedValue(), pattern.getValue());
    }

    void PatternDrawer::visit(pl::PatternStruct& pattern) {
        bool open = true;

        if (!pattern.isInlined()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            ImGui::TableNextColumn();
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            drawTypenameColumn(pattern, "struct");
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        } else {
            ImGui::SameLine();
            ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf);
        }

        if (open) {
            pattern.forEachMember([&](auto &member){
                this->draw(member);
            });

            ImGui::TreePop();
        }
    }

    void PatternDrawer::visit(pl::PatternUnion& pattern) {
        bool open = true;

        if (!pattern.isInlined()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            ImGui::TableNextColumn();
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            drawTypenameColumn(pattern, "union");
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        } else {
            ImGui::SameLine();
            ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf);
        }

        if (open) {
            pattern.forEachMember([&](auto &member) {
                this->draw(member);
            });

            ImGui::TreePop();
        }
    }

    void PatternDrawer::visit(pl::PatternUnsigned& pattern) {
        this->createDefaultEntry(pattern, pattern.getFormattedValue(), pattern.getValue());
    }

    void PatternDrawer::visit(pl::PatternWideCharacter& pattern) {
        this->createDefaultEntry(pattern, pattern.getFormattedValue(), u128(pattern.getValue()));
    }

    void PatternDrawer::visit(pl::PatternWideString& pattern) {
        std::string utf8String = pattern.getValue();

        this->createDefaultEntry(pattern, pattern.getFormattedValue(), utf8String);
    }

    void PatternDrawer::createDefaultEntry(const pl::Pattern &pattern, const std::string &value, pl::Token::Literal &&literal) const {
        ImGui::TableNextRow();
        createLeafNode(pattern);
        ImGui::TableNextColumn();

        makeSelectable(pattern);

        drawCommentTooltip(pattern);
        ImGui::SameLine();
        drawNameColumn(pattern);
        drawColorColumn(pattern);
        drawOffsetColumn(pattern);
        drawSizeColumn(pattern);
        ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{}", pattern.getFormattedName().empty() ? pattern.getTypeName() : pattern.getFormattedName());
        ImGui::TableNextColumn();
        ImGui::TextFormatted("{}", pattern.formatDisplayValue(value, literal));
    }

    void PatternDrawer::makeSelectable(const pl::Pattern &pattern) const {
        ImGui::PushID(static_cast<int>(pattern.getOffset()));
        ImGui::PushID(pattern.getVariableName().c_str());
        if (ImGui::Selectable("##PatternLine", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
            ImHexApi::HexEditor::setSelection(pattern.getOffset(), pattern.getSize());
        }
        ImGui::SameLine();
        ImGui::PopID();
        ImGui::PopID();
    }


    void PatternDrawer::drawCommentTooltip(const pl::Pattern &pattern) const {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && pattern.getComment().has_value()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(pattern.getComment()->c_str());
            ImGui::EndTooltip();
        }
    }

    void PatternDrawer::draw(pl::Pattern& pattern) {
        if (pattern.isHidden())
            return;

        pattern.accept(*this);
    }

    bool PatternDrawer::drawArrayRoot(pl::Pattern& pattern, size_t entryCount, bool isInlined) {
        if (entryCount == 0)
            return false;

        bool open = true;
        if (!isInlined) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            ImGui::TableNextColumn();
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{0}", pattern.getTypeName());
            ImGui::SameLine(0, 0);

            ImGui::TextUnformatted("[");
            ImGui::SameLine(0, 0);
            ImGui::TextFormattedColored(ImColor(0xFF00FF00), "{0}", entryCount);
            ImGui::SameLine(0, 0);
            ImGui::TextUnformatted("]");

            ImGui::TableNextColumn();
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        } else {
            ImGui::SameLine();
            ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf);
        }

        return open;
    }

    void PatternDrawer::drawArrayNode(u64 idx, u64& displayEnd, pl::Pattern& pattern) {
        u64 lastVisible = displayEnd - 1;

        ImGui::PushID(pattern.getOffset());

        if (idx < lastVisible) {
            this->draw(pattern);
        } else if (idx == lastVisible) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::Selectable("... (Double-click to see more items)", false, ImGuiSelectableFlags_SpanAllColumns);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                displayEnd += DisplayEndStep;
        }

        ImGui::PopID();
    }

    void PatternDrawer::drawArrayEnd(pl::Pattern& pattern, bool opened) {
        if (opened) {
            ImGui::TreePop();
        } else {
            auto& displayEnd = this->getDisplayEnd(pattern);
            displayEnd = DisplayEndDefault;
        }
    }

    void PatternDrawer::createLeafNode(const pl::Pattern& pattern) const {
        ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_Leaf |
                                                            ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                            ImGuiTreeNodeFlags_SpanFullWidth |
                                                            ImGuiTreeNodeFlags_AllowItemOverlap);
    }

    bool PatternDrawer::createTreeNode(const pl::Pattern& pattern) const {
        return ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
    }

    void PatternDrawer::drawTypenameColumn(const pl::Pattern& pattern, const std::string& pattern_name) const {
        ImGui::TextFormattedColored(ImColor(0xFFD69C56), pattern_name);
        ImGui::SameLine();
        ImGui::TextUnformatted(pattern.getTypeName().c_str());
        ImGui::TableNextColumn();
    }

    void PatternDrawer::drawNameColumn(const pl::Pattern& pattern) const {
        ImGui::TextUnformatted(pattern.getDisplayName().c_str());
        ImGui::TableNextColumn();
    }

    void PatternDrawer::drawColorColumn(const pl::Pattern& pattern) const {
        ImGui::ColorButton("color", ImColor(pattern.getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
        ImGui::TableNextColumn();
    }

    void PatternDrawer::drawOffsetColumn(const pl::Pattern& pattern) const {
        ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", pattern.getOffset(), pattern.getOffset() + pattern.getSize() - (pattern.getSize() == 0 ? 0 : 1));
        ImGui::TableNextColumn();
    }

    void PatternDrawer::drawSizeColumn(const pl::Pattern& pattern) const {
        ImGui::TextFormatted("0x{0:04X}", pattern.getSize());
        ImGui::TableNextColumn();
    }

    u64& PatternDrawer::getDisplayEnd(const pl::Pattern& pattern) {
        auto it = m_displayEnd.find(&pattern);
        if (it != m_displayEnd.end()) {
            return it->second;
        }

        auto [inserted, success] = m_displayEnd.emplace(&pattern, DisplayEndDefault);
        return inserted->second;
    }
};
