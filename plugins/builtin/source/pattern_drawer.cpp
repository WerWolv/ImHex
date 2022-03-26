#include "pattern_drawer.hpp"

#include <hex/pattern_language/patterns/pattern_array_dynamic.hpp>
#include <hex/pattern_language/patterns/pattern_array_static.hpp>
#include <hex/pattern_language/patterns/pattern_bitfield.hpp>
#include <hex/pattern_language/patterns/pattern_boolean.hpp>
#include <hex/pattern_language/patterns/pattern_character.hpp>
#include <hex/pattern_language/patterns/pattern_enum.hpp>
#include <hex/pattern_language/patterns/pattern_float.hpp>
#include <hex/pattern_language/patterns/pattern_padding.hpp>
#include <hex/pattern_language/patterns/pattern_pointer.hpp>
#include <hex/pattern_language/patterns/pattern_signed.hpp>
#include <hex/pattern_language/patterns/pattern_string.hpp>
#include <hex/pattern_language/patterns/pattern_struct.hpp>
#include <hex/pattern_language/patterns/pattern_union.hpp>
#include <hex/pattern_language/patterns/pattern_unsigned.hpp>
#include <hex/pattern_language/patterns/pattern_wide_character.hpp>
#include <hex/pattern_language/patterns/pattern_wide_string.hpp>

#include <string>

namespace {
    constexpr static auto DisplayEndDefault = 50u;
    constexpr static auto DisplayEndStep = 50u;

    using namespace ::std::literals::string_literals;
};

namespace hex {

    PatternDrawer::PatternDrawer()
        : m_provider{nullptr}
    { }

    void PatternDrawer::setProvider(prv::Provider *provider) {
        m_provider = provider;
    }

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

        u64 extractedValue = pattern.getValue(m_provider);
        ImGui::TextFormatted("{}", pattern.formatDisplayValue(hex::format("{0} (0x{1:X})", extractedValue, extractedValue), &pattern));
    }

    void PatternDrawer::visit(pl::PatternBitfield& pattern) {
        std::vector<u8> value = pattern.getValue(m_provider);

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

            std::string valueString = "{ ";
            for (auto i : value)
                valueString += hex::format("{0:02X} ", i);
            valueString += "}";

            ImGui::TextFormatted("{}", pattern.formatDisplayValue(valueString, &pattern));
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
        u8 boolean = pattern.getValue(m_provider);

        if (boolean == 0)
            this->createDefaultEntry(pattern, "false", false);
        else if (boolean == 1)
            this->createDefaultEntry(pattern, "true", true);
        else
            this->createDefaultEntry(pattern, "true*", true);
    }

    void PatternDrawer::visit(pl::PatternCharacter& pattern) {
        char character = pattern.getValue(m_provider);
        this->createDefaultEntry(pattern, hex::format("'{0}'", character), character);
    }

    void PatternDrawer::visit(pl::PatternEnum& pattern) {
        u64 value = pattern.getValue(m_provider);

        std::string valueString = pattern.getTypeName() + "::";

        bool foundValue = false;
        for (auto &[entryValueLiteral, entryName] : pattern.getEnumValues()) {
            auto visitor = overloaded {
                [&, name = entryName](auto &entryValue) {
                    if (static_cast<decltype(entryValue)>(value) == entryValue) {
                        valueString += name;
                        foundValue = true;
                        return true;
                    }

                    return false;
                },
                [](const std::string &) { return false; },
                [](pl::Pattern *) { return false; },
            };

            bool matches = std::visit(visitor, entryValueLiteral);
            if (matches)
                break;
        }

        if (!foundValue)
            valueString += "???";

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
        ImGui::TextFormatted("{}", pattern.formatDisplayValue(hex::format("{} (0x{:0{}X})", valueString.c_str(), value, pattern.getSize() * 2), &pattern));
    }

    void PatternDrawer::visit(pl::PatternFloat& pattern) {
        if (pattern.getSize() == 4) {
            float f32 = static_cast<float>(pattern.getValue(m_provider));
            u32 integerResult = 0;
            std::memcpy(&integerResult, &f32, sizeof(float));
            this->createDefaultEntry(pattern, hex::format("{:e} (0x{:0{}X})", f32, integerResult, pattern.getSize() * 2), f32);
        } else if (pattern.getSize() == 8) {
            double f64 = pattern.getValue(m_provider);
            u64 integerResult = 0;
            std::memcpy(&integerResult, &f64, sizeof(double));
            this->createDefaultEntry(pattern, hex::format("{:e} (0x{:0{}X})", f64, integerResult, pattern.getSize() * 2), f64);
        }
    }

    void PatternDrawer::visit(pl::PatternPadding& pattern) {
        // Do nothing
        hex::unused(pattern);
    }

    void PatternDrawer::visit(pl::PatternPointer& pattern) {
        u64 data = pattern.getValue(m_provider);

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
            ImGui::TextFormatted("{}", pattern.formatDisplayValue(hex::format("*(0x{0:X})", data), u128(data)));
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
        i128 data = pattern.getValue(m_provider);
        this->createDefaultEntry(pattern, hex::format("{:d} (0x{:0{}X})", data, data, 1 * 2), data);
    }

    void PatternDrawer::visit(pl::PatternString& pattern) {
        auto size = std::min<size_t>(pattern.getSize(), 0x7F);

        if (size == 0)
            return;

        std::string displayString = pattern.getValue(m_provider, size);
        this->createDefaultEntry(pattern, hex::format("\"{0}\" {1}", displayString, size > pattern.getSize() ? "(truncated)" : ""), displayString);
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
            ImGui::TextFormatted("{}", pattern.formatDisplayValue("{ ... }", &pattern));
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
            ImGui::TextFormatted("{}", pattern.formatDisplayValue("{ ... }", &pattern));
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
        u128 data = pattern.getValue(m_provider);
        this->createDefaultEntry(pattern, hex::format("{:d} (0x{:0{}X})", data, data, pattern.getSize() * 2), data);
    }

    void PatternDrawer::visit(pl::PatternWideCharacter& pattern) {
        char16_t character = pattern.getValue(m_provider);
        u128 literal = character;
        auto str = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> {}.to_bytes(character);
        this->createDefaultEntry(pattern, hex::format("'{0}'", str), literal);
    }

    void PatternDrawer::visit(pl::PatternWideString& pattern) {
        auto size = std::min<size_t>(pattern.getSize(), 0x100);

        if (size == 0)
            return;

        std::string utf8String = pattern.getValue(m_provider, size);

        this->createDefaultEntry(pattern, hex::format("\"{0}\" {1}", utf8String, size > pattern.getSize() ? "(truncated)" : ""), utf8String);
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
        ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{}", pattern.getTypeName().empty() ? pattern.getFormattedName() : pattern.getTypeName());
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
            ImGui::TextFormatted("{}", pattern.formatDisplayValue("{ ... }", &pattern));
        } else {
            ImGui::SameLine();
            ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf);
        }

        return open;
    }

    void PatternDrawer::drawArrayNode(u64 idx, u64 displayEnd, pl::Pattern& pattern) {
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
