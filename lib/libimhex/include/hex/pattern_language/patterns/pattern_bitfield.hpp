#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

namespace hex::pl {

    class PatternBitfieldField : public Pattern {
    public:
        PatternBitfieldField(Evaluator *evaluator, u64 offset, u8 bitOffset, u8 bitSize, Pattern *bitField, u32 color = 0)
            : Pattern(evaluator, offset, 0, color), m_bitOffset(bitOffset), m_bitSize(bitSize), m_bitField(bitField) {
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBitfieldField(*this));
        }

        void createEntry(prv::Provider *&provider) override {
            std::vector<u8> value(this->m_bitField->getSize(), 0);
            provider->read(this->m_bitField->getOffset(), &value[0], value.size());

            if (this->m_bitField->getEndian() != std::endian::native)
                std::reverse(value.begin(), value.end());

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(this->getDisplayName().c_str());
            ImGui::SameLine();
            if (ImGui::Selectable(("##PatternLine"s + std::to_string(u64(this))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                ImHexApi::HexEditor::setSelection(this->getOffset(), this->getSize());
            }
            ImGui::TableNextColumn();
            ImGui::ColorButton("color", ImColor(this->getColor()), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
            ImGui::TableNextColumn();
            if (this->m_bitSize == 1)
                ImGui::TextFormatted("0x{0:08X} bit {1}", this->getOffset() + this->m_bitOffset / 8, (this->m_bitOffset + (this->m_bitSize - 1)) % 8);
            else
                ImGui::TextFormatted("0x{0:08X} bits {1} - {2}", this->getOffset() + this->m_bitOffset / 8, this->m_bitOffset % 8, this->m_bitOffset % 8 + (this->m_bitSize - 1) % 8);
            ImGui::TableNextColumn();
            if (this->m_bitSize == 1)
                ImGui::TextFormatted("{0} bit", this->m_bitSize);
            else
                ImGui::TextFormatted("{0} bits", this->m_bitSize);
            ImGui::TableNextColumn();
            ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "bits");
            ImGui::TableNextColumn();
            {
                u8 numBytes = (this->m_bitSize / 8) + 1;

                u64 extractedValue = hex::extract(this->m_bitOffset + (this->m_bitSize - 1), this->m_bitOffset, value);
                ImGui::TextFormatted("{}", this->formatDisplayValue(hex::format("{0} (0x{1:X})", extractedValue, extractedValue), this));
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bits";
        }

        [[nodiscard]] u8 getBitOffset() const {
            return this->m_bitOffset;
        }

        [[nodiscard]] u8 getBitSize() const {
            return this->m_bitSize;
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherBitfieldField = *static_cast<const PatternBitfieldField *>(&other);
            return this->m_bitOffset == otherBitfieldField.m_bitOffset && this->m_bitSize == otherBitfieldField.m_bitSize;
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

    private:
        u8 m_bitOffset, m_bitSize;
        Pattern *m_bitField;
    };

    class PatternBitfield : public Pattern,
                            public Inlinable {
    public:
        PatternBitfield(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) {
        }

        PatternBitfield(const PatternBitfield &other) : Pattern(other) {
            for (auto &field : other.m_fields)
                this->m_fields.push_back(field->clone());
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBitfield(*this));
        }

        void createEntry(prv::Provider *&provider) override {
            std::vector<u8> value(this->getSize(), 0);
            provider->read(this->getOffset(), &value[0], value.size());

            if (this->m_endian == std::endian::little)
                std::reverse(value.begin(), value.end());

            bool open = true;
            if (!this->isInlined()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                open = ImGui::TreeNodeEx(this->getDisplayName().c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(("##PatternLine"s + std::to_string(u64(this))).c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
                    ImHexApi::HexEditor::setSelection(this->getOffset(), this->getSize());
                }
                this->drawCommentTooltip();
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", this->getOffset(), this->getOffset() + this->getSize() - 1);
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", this->getSize());
                ImGui::TableNextColumn();
                ImGui::TextFormattedColored(ImColor(0xFFD69C56), "bitfield");
                ImGui::SameLine();
                ImGui::TextUnformatted(Pattern::getTypeName().c_str());
                ImGui::TableNextColumn();

                std::string valueString = "{ ";
                for (auto i : value)
                    valueString += hex::format("{0:02X} ", i);
                valueString += "}";

                ImGui::TextFormatted("{}", this->formatDisplayValue(valueString, this));
            } else {
                ImGui::SameLine();
                ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf);
            }

            if (open) {

                for (auto &field : this->m_fields)
                    field->draw(provider);

                ImGui::TreePop();
            }

        }

        void forEachMember(const std::function<void(Pattern&)>& fn) {
            for (auto &field : this->m_fields)
                fn(*field);
        }

        void setOffset(u64 offset) override {
            for (auto &field : this->m_fields)
                field->setOffset(field->getOffset() - this->getOffset() + offset);

            Pattern::setOffset(offset);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bitfield " + Pattern::getTypeName();
        }

        [[nodiscard]] const auto &getFields() const {
            return this->m_fields;
        }

        void setFields(std::vector<std::shared_ptr<Pattern>> &&fields) {
            this->m_fields = std::move(fields);

            for (auto &field : this->m_fields) {
                field->setSize(this->getSize());
                field->setColor(this->getColor());
            }
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            for (auto &field : this->m_fields)
                field->setColor(color);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherBitfield = *static_cast<const PatternBitfield *>(&other);
            if (this->m_fields.size() != otherBitfield.m_fields.size())
                return false;

            for (u64 i = 0; i < this->m_fields.size(); i++) {
                if (*this->m_fields[i] != *otherBitfield.m_fields[i])
                    return false;
            }

            return true;
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

    private:
        std::vector<std::shared_ptr<Pattern>> m_fields;
    };

}