#pragma once

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

#include <nlohmann/json.hpp>

namespace hex::pl {

    class ImGuiDrawer : public PatternVisitor
    {
    public:
        ImGuiDrawer(prv::Provider *provider)
            : m_provider{provider}
        { }

        void visit(PatternArrayDynamic& pattern) override {
            if (pattern.getEntryCount() == 0)
                return;

            bool open = true;
            if (!pattern.isInlined()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                open = ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(("##PatternLine"s + std::to_string(u64(&pattern))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    ImHexApi::HexEditor::setSelection(pattern.getOffset(), pattern.getSize());
                }
                this->drawCommentTooltip(pattern);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", pattern.getOffset(), pattern.getOffset() + pattern.getSize() - 1);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", pattern.getSize());
                ImGui::TableNextColumn();
                ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{0}", pattern.getTypeName());
                ImGui::SameLine(0, 0);

                ImGui::TextUnformatted("[");
                ImGui::SameLine(0, 0);
                ImGui::TextFormattedColored(ImColor(0xFF00FF00), "{0}", pattern.getEntryCount());
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted("]");

                ImGui::TableNextColumn();
                ImGui::TextFormatted("{}", pattern.formatDisplayValue("{ ... }", &pattern));
            } else {
                ImGui::SameLine();
                ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf);
            }

            if (open) {
                pattern.forEachArrayEntry([&] (u64 idx, auto &entry){
                    u64 lastVisible = pattern.getDisplayEnd() - 1;
                    if (idx < lastVisible) {
                        this->draw(entry);
                    } else if (idx == lastVisible) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::Selectable("... (Double-click to see more items)", false, ImGuiSelectableFlags_SpanAllColumns);
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            pattern.increaseDisplayEnd();
                    }
                });

                ImGui::TreePop();
            } else {
                pattern.resetDisplayEnd();
            }
        }

        void visit(PatternArrayStatic& pattern) override {
            if (pattern.getEntryCount() == 0)
                return;

            bool open = true;

            if (!pattern.isInlined()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                open = ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(("##PatternLine"s + std::to_string(u64(&pattern))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    ImHexApi::HexEditor::setSelection(pattern.getOffset(), pattern.getSize());
                }
                this->drawCommentTooltip(pattern);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", pattern.getOffset(), pattern.getOffset() + pattern.getSize() - 1);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", pattern.getSize());
                ImGui::TableNextColumn();
                ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{0}", pattern.getTypeName().c_str());
                ImGui::SameLine(0, 0);

                ImGui::TextUnformatted("[");
                ImGui::SameLine(0, 0);
                ImGui::TextFormattedColored(ImColor(0xFF00FF00), "{0}", pattern.getEntryCount());
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted("]");

                ImGui::TableNextColumn();
                ImGui::TextFormatted("{}", pattern.formatDisplayValue("{ ... }", &pattern));
            } else {
                ImGui::SameLine();
                ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf);
            }

            if (open) {
                pattern.forEachArrayEntry([&] (u64 index, auto &entry){
                    u64 lastVisible = pattern.getDisplayEnd();
                    if (index < lastVisible) {
                        this->draw(entry);
                    } else if (index == lastVisible) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::Selectable("... (Double-click to see more items)", false, ImGuiSelectableFlags_SpanAllColumns);
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            pattern.increaseDisplayEnd();
                    }
                });

                ImGui::TreePop();
            } else {
                pattern.resetDisplayEnd();
            }
        }

        void visit(PatternBitfieldField& pattern) override {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(pattern.getDisplayName().c_str());
            ImGui::SameLine();
            if (ImGui::Selectable(("##PatternLine"s + std::to_string(u64(&pattern))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                ImHexApi::HexEditor::setSelection(pattern.getOffset(), pattern.getSize());
            }
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(pattern.getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();

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

        void visit(PatternBitfield& pattern) override {
            std::vector<u8> value = pattern.getValue(m_provider);

            bool open = true;
            if (!pattern.isInlined()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                open = ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(("##PatternLine"s + std::to_string(u64(&pattern))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    ImHexApi::HexEditor::setSelection(pattern.getOffset(), pattern.getSize());
                }
                this->drawCommentTooltip(pattern);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", pattern.getOffset(), pattern.getOffset() + pattern.getSize() - 1);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", pattern.getSize());
                ImGui::TableNextColumn();
                ImGui::TextFormattedColored(ImColor(0xFFD69C56), "bitfield");
                ImGui::SameLine();
                ImGui::TextUnformatted(pattern.getTypeName().c_str());
                ImGui::TableNextColumn();

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

        void visit(PatternBoolean& pattern) override {
            u8 boolean = pattern.getValue(m_provider);

            if (boolean == 0)
                this->createDefaultEntry(pattern, "false", false);
            else if (boolean == 1)
                this->createDefaultEntry(pattern, "true", true);
            else
                this->createDefaultEntry(pattern, "true*", true);
        }

        void visit(PatternCharacter& pattern) override {
            char character = pattern.getValue(m_provider);
            this->createDefaultEntry(pattern, hex::format("'{0}'", character), character);
        }

        void visit(PatternEnum& pattern) override {
            u64 value = pattern.getValue(m_provider);

            std::string valueString = pattern.getTypeName() + "::";

            bool foundValue = false;
            for (auto &[entryValueLiteral, entryName] : pattern.getEnumValues()) {
                auto visitor = overloaded {
                    [&, name = entryName](auto &entryValue) {
                        if (value == entryValue) {
                            valueString += name;
                            foundValue = true;
                            return true;
                        }

                        return false;
                    },
                    [](const std::string &) { return false; },
                    [](Pattern *) { return false; },
                };

                bool matches = std::visit(visitor, entryValueLiteral);
                if (matches)
                    break;
            }

            if (!foundValue)
                valueString += "???";

            ImGui::TableNextRow();
            ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_Leaf |
                                                                ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                                                ImGuiTreeNodeFlags_SpanFullWidth |
                                                                ImGuiTreeNodeFlags_AllowItemOverlap);
            this->drawCommentTooltip(pattern);
            ImGui::TableNextColumn();
            if (ImGui::Selectable(("##PatternLine"s + std::to_string(u64(&pattern))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                ImHexApi::HexEditor::setSelection(pattern.getOffset(), pattern.getSize());
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(pattern.getDisplayName().c_str());
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(pattern.getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
            ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", pattern.getOffset(), pattern.getOffset() + pattern.getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::TextFormatted("0x{0:04X}", pattern.getSize());
            ImGui::TableNextColumn();
            ImGui::TextFormattedColored(ImColor(0xFFD69C56), "enum");
            ImGui::SameLine();
            ImGui::TextUnformatted(pattern.getTypeName().c_str());
            ImGui::TableNextColumn();
            ImGui::TextFormatted("{}", pattern.formatDisplayValue(hex::format("{} (0x{:0{}X})", valueString.c_str(), value, pattern.getSize() * 2), &pattern));
        }

        void visit(PatternFloat& pattern) override {
            if (pattern.getSize() == 4) {
                float f32 = pattern.getValue(m_provider);
                u32 data = *reinterpret_cast<u32 *>(&f32);
                this->createDefaultEntry(pattern, hex::format("{:e} (0x{:0{}X})", f32, data, pattern.getSize() * 2), f32);
            } else if (pattern.getSize() == 8) {
                double f64 = pattern.getValue(m_provider);
                u64 data = *reinterpret_cast<u64 *>(&f64);
                this->createDefaultEntry(pattern, hex::format("{:e} (0x{:0{}X})", f64, data, pattern.getSize() * 2), f64);
            }
        }

        void visit(PatternPadding& pattern) override {
            // Do nothing
        }

        void visit(PatternPointer& pattern) override {
            u64 data = pattern.getValue(m_provider);

            bool open = true;

            if (!pattern.isInlined()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                open = ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(("##PatternLine"s + std::to_string(u64(&pattern))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    ImHexApi::HexEditor::setSelection(pattern.getOffset(), pattern.getSize());
                }
                this->drawCommentTooltip(pattern);
                ImGui::SameLine(0, 0);
                ImGui::ColorButton("color", ImColor(pattern.getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", pattern.getOffset(), pattern.getOffset() + pattern.getSize() - 1);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", pattern.getSize());
                ImGui::TableNextColumn();
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

        void visit(PatternSigned& pattern) override {
            i128 data = pattern.getValue(m_provider);
            this->createDefaultEntry(pattern, hex::format("{:d} (0x{:0{}X})", data, data, 1 * 2), data);
        }

        void visit(PatternString& pattern) override {
            auto size = std::min<size_t>(pattern.getSize(), 0x7F);

            if (size == 0)
                return;

            std::string displayString = pattern.getValue(m_provider, size);
            this->createDefaultEntry(pattern, hex::format("\"{0}\" {1}", displayString, size > pattern.getSize() ? "(truncated)" : ""), displayString);
        }

        void visit(PatternStruct& pattern) override {
            bool open = true;

            if (!pattern.isInlined()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                open = ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(("##PatternLine"s + std::to_string(u64(&pattern))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    ImHexApi::HexEditor::setSelection(pattern.getOffset(), pattern.getSize());
                }
                this->drawCommentTooltip(pattern);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", pattern.getOffset(), pattern.getOffset() + pattern.getSize() - (pattern.getSize() == 0 ? 0 : 1));
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", pattern.getSize());
                ImGui::TableNextColumn();
                ImGui::TextFormattedColored(ImColor(0xFFD69C56), "struct");
                ImGui::SameLine();
                ImGui::TextUnformatted(pattern.getTypeName().c_str());
                ImGui::TableNextColumn();
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

        void visit(PatternUnion& pattern) override {
            bool open = true;

            if (!pattern.isInlined()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                open = ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(("##PatternLine"s + std::to_string(u64(&pattern))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    ImHexApi::HexEditor::setSelection(pattern.getOffset(), pattern.getSize());
                }
                this->drawCommentTooltip(pattern);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", pattern.getOffset(), std::max(pattern.getOffset() + pattern.getSize() - (pattern.getSize() == 0 ? 0 : 1), u64(0)));
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", pattern.getSize());
                ImGui::TableNextColumn();
                ImGui::TextFormattedColored(ImColor(0xFFD69C56), "union");
                ImGui::SameLine();
                ImGui::TextUnformatted(pattern.getTypeName().c_str());

                ImGui::TableNextColumn();
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

        void visit(PatternUnsigned& pattern) override {
            u128 data = pattern.getValue(m_provider);
            this->createDefaultEntry(pattern, hex::format("{:d} (0x{:0{}X})", data, data, pattern.getSize() * 2), data);
        }

        void visit(PatternWideCharacter& pattern) override {
            char16_t character = pattern.getValue(m_provider);
            u128 literal = character;
            auto str = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> {}.to_bytes(character);
            this->createDefaultEntry(pattern, hex::format("'{0}'", str), literal);
        }

        void visit(PatternWideString& pattern) override {
            auto size = std::min<size_t>(pattern.getSize(), 0x100);

            if (size == 0)
                return;

            std::string utf8String = pattern.getValue(m_provider, size);

            this->createDefaultEntry(pattern, hex::format("\"{0}\" {1}", utf8String, size > pattern.getSize() ? "(truncated)" : ""), utf8String);
        }

    private:
        void createDefaultEntry(const Pattern &pattern, const std::string &value, Token::Literal &&literal) const {
            ImGui::TableNextRow();
            ImGui::TreeNodeEx(pattern.getDisplayName().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap);
            ImGui::TableNextColumn();

            ImGui::PushID(pattern.getOffset());
            ImGui::PushID(pattern.getVariableName().c_str());
            if (ImGui::Selectable("##PatternLine", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                ImHexApi::HexEditor::setSelection(pattern.getOffset(), pattern.getSize());
            }
            ImGui::PopID();
            ImGui::PopID();

            this->drawCommentTooltip(pattern);
            ImGui::SameLine();
            ImGui::TextUnformatted(pattern.getDisplayName().c_str());
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(pattern.getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
            ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", pattern.getOffset(), pattern.getOffset() + pattern.getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::TextFormatted("0x{0:04X}", pattern.getSize());
            ImGui::TableNextColumn();
            ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{}", pattern.getTypeName().empty() ? pattern.getFormattedName() : pattern.getTypeName());
            ImGui::TableNextColumn();
            ImGui::TextFormatted("{}", pattern.formatDisplayValue(value, literal));
        }

        void drawCommentTooltip(const Pattern &pattern) const {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && pattern.getComment().has_value()) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(pattern.getComment()->c_str());
                ImGui::EndTooltip();
            }
        }

        void draw(Pattern& pattern) {
            if (pattern.isHidden())
                return;

            pattern.accept(*this);
        }

    private:
        prv::Provider *m_provider;
    };
};