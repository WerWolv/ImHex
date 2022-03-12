#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

namespace hex::pl {

    class PatternArrayDynamic : public Pattern,
                                public Inlinable {
    public:
        PatternArrayDynamic(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) {
        }

        PatternArrayDynamic(const PatternArrayDynamic &other) : Pattern(other) {
            std::vector<std::shared_ptr<Pattern>> entries;
            for (const auto &entry : other.m_entries)
                entries.push_back(entry->clone());

            this->setEntries(std::move(entries));
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternArrayDynamic(*this));
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            for (auto &entry : this->m_entries)
                entry->setColor(color);
        }

        void createEntry(prv::Provider *&provider) override {
            if (this->m_entries.empty())
                return;

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
                ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{0}", this->m_entries[0]->getTypeName());
                ImGui::SameLine(0, 0);

                ImGui::TextUnformatted("[");
                ImGui::SameLine(0, 0);
                ImGui::TextFormattedColored(ImColor(0xFF00FF00), "{0}", this->m_entries.size());
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted("]");

                ImGui::TableNextColumn();
                ImGui::TextFormatted("{}", this->formatDisplayValue("{ ... }", this));
            } else {
                ImGui::SameLine();
                ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf);
            }

            if (open) {
                for (u64 i = 0; i < this->m_entries.size(); i++) {
                    this->m_entries[i]->draw(provider);

                    if (i >= (this->m_displayEnd - 1)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::Selectable("... (Double-click to see more items)", false, ImGuiSelectableFlags_SpanAllColumns);
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            this->m_displayEnd += 50;

                        break;
                    }
                }

                ImGui::TreePop();
            } else {
                this->m_displayEnd = 50;
            }
        }

        void getHighlightedAddresses(std::map<u64, u32> &highlight) const override {
            for (auto &entry : this->m_entries) {
                entry->getHighlightedAddresses(highlight);
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->m_entries[0]->getTypeName() + "[" + std::to_string(this->m_entries.size()) + "]";
        }

        void setOffset(u64 offset) override {
            for (auto &entry : this->m_entries)
                entry->setOffset(entry->getOffset() - this->getOffset() + offset);

            Pattern::setOffset(offset);
        }

        [[nodiscard]] const auto &getEntries() {
            return this->m_entries;
        }

        void setEntries(std::vector<std::shared_ptr<Pattern>> &&entries) {
            this->m_entries = std::move(entries);

            for (auto &entry : this->m_entries) {
                entry->setBaseColor(this->getColor());
            }
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherArray = *static_cast<const PatternArrayDynamic *>(&other);
            if (this->m_entries.size() != otherArray.m_entries.size())
                return false;

            for (u64 i = 0; i < this->m_entries.size(); i++) {
                if (*this->m_entries[i] != *otherArray.m_entries[i])
                    return false;
            }

            return true;
        }

        [[nodiscard]] const Pattern *getPattern(u64 offset) const override {
            if (this->isHidden()) return nullptr;

            for (auto &pattern : this->m_entries) {
                auto result = pattern->getPattern(offset);
                if (result != nullptr)
                    return result;
            }

            return nullptr;
        }

        void setEndian(std::endian endian) override {
            for (auto &entry : this->m_entries) {
                entry->setEndian(endian);
            }

            Pattern::setEndian(endian);
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

    private:
        std::vector<std::shared_ptr<Pattern>> m_entries;
        u64 m_displayEnd = 50;
    };

}