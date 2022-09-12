#include <content/helpers/pattern_drawer.hpp>

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

namespace hex {

    namespace {

        constexpr auto DisplayEndDefault = 50U;
        constexpr auto DisplayEndStep = 50U;

        using namespace ::std::literals::string_literals;

        void createLeafNode(const pl::ptrn::Pattern& pattern) {
            ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_Leaf |
                                                                ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                                ImGuiTreeNodeFlags_SpanFullWidth |
                                                                ImGuiTreeNodeFlags_AllowItemOverlap);
        }

        bool createTreeNode(const pl::ptrn::Pattern& pattern) {
            if (pattern.isSealed()) {
                ImGui::Indent();
                ImGui::TextUnformatted(pattern.getDisplayName().c_str());
                ImGui::Unindent();
                return false;
            }
            else
                return ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
        }

        void drawTypenameColumn(const pl::ptrn::Pattern& pattern, const std::string& pattern_name) {
            ImGui::TextFormattedColored(ImColor(0xFFD69C56), pattern_name);
            ImGui::SameLine();
            ImGui::TextUnformatted(pattern.getTypeName().c_str());
            ImGui::TableNextColumn();
        }

        void drawNameColumn(const pl::ptrn::Pattern& pattern) {
            ImGui::TextUnformatted(pattern.getDisplayName().c_str());
            ImGui::TableNextColumn();
        }

        void drawColorColumn(const pl::ptrn::Pattern& pattern) {
            ImGui::ColorButton("color", ImColor(pattern.getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
        }

        void drawOffsetColumn(const pl::ptrn::Pattern& pattern) {
            ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", pattern.getOffset(), pattern.getOffset() + pattern.getSize() - (pattern.getSize() == 0 ? 0 : 1));
            ImGui::TableNextColumn();
        }

        void drawSizeColumn(const pl::ptrn::Pattern& pattern) {
            ImGui::TextFormatted("0x{0:04X}", pattern.getSize());
            ImGui::TableNextColumn();
        }

        void drawCommentTooltip(const pl::ptrn::Pattern &pattern) {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && pattern.getComment() != nullptr) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(pattern.getComment()->c_str());
                ImGui::EndTooltip();
            }
        }

        void makeSelectable(const pl::ptrn::Pattern &pattern) {
            ImGui::PushID(static_cast<int>(pattern.getOffset()));
            ImGui::PushID(pattern.getVariableName().c_str());

            if (ImGui::Selectable("##PatternLine", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                ImHexApi::HexEditor::setSelection(pattern.getOffset(), pattern.getSize());
            }

            ImGui::SameLine(0, 0);

            ImGui::PopID();
            ImGui::PopID();
        }

        void createDefaultEntry(pl::ptrn::Pattern &pattern) {
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
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }

    }

    void PatternDrawer::visit(pl::ptrn::PatternArrayDynamic& pattern) {
        this->drawArray(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternArrayStatic& pattern) {
        this->drawArray(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternBitfieldField& pattern) {
        ImGui::TableNextRow();
        createLeafNode(pattern);
        ImGui::TableNextColumn();

        makeSelectable(pattern);
        drawCommentTooltip(pattern);
        ImGui::SameLine();
        drawNameColumn(pattern);
        drawColorColumn(pattern);

        auto byteAddr = pattern.getOffset() + pattern.getBitOffset() / 8;
        auto firstBitIdx = pattern.getBitOffset() % 8;
        auto lastBitIdx = firstBitIdx + (pattern.getBitSize() - 1);
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

    void PatternDrawer::visit(pl::ptrn::PatternBitfield& pattern) {
        bool open = true;
        if (!pattern.isInlined()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            drawColorColumn(pattern);
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            drawTypenameColumn(pattern, "bitfield");

            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }

        if (open) {
            pattern.forEachMember([&] (auto &field) {
                this->draw(field);
            });

            if (!pattern.isInlined())
                ImGui::TreePop();
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternBoolean& pattern) {
        createDefaultEntry(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternCharacter& pattern) {
        createDefaultEntry(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternEnum& pattern) {
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

    void PatternDrawer::visit(pl::ptrn::PatternFloat& pattern) {
        createDefaultEntry(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternPadding& pattern) {
        // Do nothing
        hex::unused(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternPointer& pattern) {
        bool open = true;

        if (!pattern.isInlined()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            drawColorColumn(pattern);
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{}", pattern.getFormattedName());
            ImGui::TableNextColumn();
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }

        if (open) {
            pattern.getPointedAtPattern()->accept(*this);

            if (!pattern.isInlined())
                ImGui::TreePop();
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternSigned& pattern) {
        createDefaultEntry(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternString& pattern) {
        if (pattern.getSize() > 0)
            createDefaultEntry(pattern);
        }

    void PatternDrawer::visit(pl::ptrn::PatternStruct& pattern) {
        bool open = true;

        if (!pattern.isInlined()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
                ImGui::TableNextColumn();
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            drawTypenameColumn(pattern, "struct");
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }

        if (open) {
            pattern.forEachMember([&](auto &member){
                this->draw(member);
            });

            if (!pattern.isInlined())
                ImGui::TreePop();
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternUnion& pattern) {
        bool open = true;

        if (!pattern.isInlined()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            open = createTreeNode(pattern);
            ImGui::TableNextColumn();
            makeSelectable(pattern);
            drawCommentTooltip(pattern);
            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
                ImGui::TableNextColumn();
            drawOffsetColumn(pattern);
            drawSizeColumn(pattern);
            drawTypenameColumn(pattern, "union");
            ImGui::TextFormatted("{}", pattern.getFormattedValue());
        }

        if (open) {
            pattern.forEachMember([&](auto &member) {
                this->draw(member);
            });

            if (!pattern.isInlined())
                ImGui::TreePop();
        }
    }

    void PatternDrawer::visit(pl::ptrn::PatternUnsigned& pattern) {
        createDefaultEntry(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternWideCharacter& pattern) {
        createDefaultEntry(pattern);
    }

    void PatternDrawer::visit(pl::ptrn::PatternWideString& pattern) {
        if (pattern.getSize() > 0)
            createDefaultEntry(pattern);
    }

    void PatternDrawer::draw(pl::ptrn::Pattern& pattern) {
        if (pattern.isHidden())
            return;

        pattern.accept(*this);
    }

    bool PatternDrawer::drawArrayRoot(pl::ptrn::Pattern& pattern, size_t entryCount, bool isInlined) {
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
            if (pattern.isSealed())
                drawColorColumn(pattern);
            else
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
        }

        return open;
    }

    void PatternDrawer::drawArrayNode(u64 idx, u64& displayEnd, pl::ptrn::Pattern& pattern) {
        u64 lastVisible = displayEnd - 1;

        ImGui::PushID(reinterpret_cast<void*>(pattern.getOffset()));

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

    void PatternDrawer::drawArrayEnd(pl::ptrn::Pattern& pattern, bool opened, bool inlined) {
        if (opened) {
            if (!inlined)
                ImGui::TreePop();
        } else {
            auto& displayEnd = this->getDisplayEnd(pattern);
            displayEnd = DisplayEndDefault;
        }
    }

    u64& PatternDrawer::getDisplayEnd(const pl::ptrn::Pattern& pattern) {
        auto it = this->m_displayEnd.find(&pattern);
        if (it != this->m_displayEnd.end()) {
            return it->second;
        }

        auto [value, success] = this->m_displayEnd.emplace(&pattern, DisplayEndDefault);
        return value->second;
    }
};
