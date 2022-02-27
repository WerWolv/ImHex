#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

namespace hex::pl {

    class PatternEnum : public Pattern {
    public:
        PatternEnum(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) {
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternEnum(*this));
        }

        void createEntry(prv::Provider *&provider) override {
            u64 value = 0;
            provider->read(this->getOffset(), &value, this->getSize());
            value = hex::changeEndianess(value, this->getSize(), this->getEndian());

            std::string valueString = Pattern::getTypeName() + "::";

            bool foundValue = false;
            for (auto &[entryValueLiteral, entryName] : this->m_enumValues) {
                bool matches = std::visit(overloaded {
                                              [&, name = entryName](auto &&entryValue) {
                                                  if (value == entryValue) {
                                                      valueString += name;
                                                      foundValue = true;
                                                      return true;
                                                  }

                                                  return false;
                                              },
                                              [](std::string &) { return false; },
                                              [](std::shared_ptr<Pattern> &) { return false; } },
                    entryValueLiteral);
                if (matches)
                    break;
            }

            if (!foundValue)
                valueString += "???";

            ImGui::TableNextRow();
            ImGui::TreeNodeEx(this->getDisplayName().c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_AllowItemOverlap);
            this->drawCommentTooltip();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(("##PatternLine"s + std::to_string(u64(this))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                ImHexApi::HexEditor::setSelection(this->getOffset(), this->getSize());
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(this->getDisplayName().c_str());
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
            ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", this->getOffset(), this->getOffset() + this->getSize() - 1);
            ImGui::TableNextColumn();
            ImGui::TextFormatted("0x{0:04X}", this->getSize());
            ImGui::TableNextColumn();
            ImGui::TextFormattedColored(ImColor(0xFFD69C56), "enum");
            ImGui::SameLine();
            ImGui::TextUnformatted(Pattern::getTypeName().c_str());
            ImGui::TableNextColumn();
            ImGui::TextFormatted("{}", this->formatDisplayValue(hex::format("{} (0x{:0{}X})", valueString.c_str(), value, this->getSize() * 2), this->clone()));
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "enum " + Pattern::getTypeName();
        }

        [[nodiscard]] const auto &getEnumValues() const {
            return this->m_enumValues;
        }

        void setEnumValues(const std::vector<std::pair<Token::Literal, std::string>> &enumValues) {
            this->m_enumValues = enumValues;
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherEnum = *static_cast<const PatternEnum *>(&other);
            if (this->m_enumValues.size() != otherEnum.m_enumValues.size())
                return false;

            for (u64 i = 0; i < this->m_enumValues.size(); i++) {
                if (this->m_enumValues[i] != otherEnum.m_enumValues[i])
                    return false;
            }

            return true;
        }

    private:
        std::vector<std::pair<Token::Literal, std::string>> m_enumValues;
    };

}