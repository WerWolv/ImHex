#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

namespace hex::pl {

    class PatternStruct : public Pattern,
                          public Inlinable {
    public:
        PatternStruct(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) {
        }

        PatternStruct(const PatternStruct &other) : Pattern(other) {
            for (const auto &member : other.m_members) {
                auto copy = member->clone();

                this->m_sortedMembers.push_back(copy.get());
                this->m_members.push_back(std::move(copy));
            }
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternStruct(*this));
        }

        void createEntry(prv::Provider *&provider) override {
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
                ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", this->getOffset(), this->getOffset() + this->getSize() - (this->getSize() == 0 ? 0 : 1));
                ImGui::TableNextColumn();
                ImGui::TextFormatted("0x{0:04X}", this->getSize());
                ImGui::TableNextColumn();
                ImGui::TextFormattedColored(ImColor(0xFFD69C56), "struct");
                ImGui::SameLine();
                ImGui::TextUnformatted(this->getTypeName().c_str());
                ImGui::TableNextColumn();
                ImGui::TextFormatted("{}", this->formatDisplayValue("{ ... }", this));
            }

            if (open) {
                for (auto &member : this->m_sortedMembers)
                    member->draw(provider);

                if (!this->isInlined())
                    ImGui::TreePop();
            }
        }

        void getHighlightedAddresses(std::map<u64, u32> &highlight) const override {
            for (auto &member : this->m_members) {
                member->getHighlightedAddresses(highlight);
            }
        }

        void setOffset(u64 offset) override {
            for (auto &member : this->m_members)
                member->setOffset(member->getOffset() - this->getOffset() + offset);

            Pattern::setOffset(offset);
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            for (auto &member : this->m_members) {
                if (!member->hasOverriddenColor())
                    member->setColor(color);
            }
        }

        void sort(ImGuiTableSortSpecs *sortSpecs, prv::Provider *provider) override {
            this->m_sortedMembers.clear();
            for (auto &member : this->m_members)
                this->m_sortedMembers.push_back(member.get());

            std::sort(this->m_sortedMembers.begin(), this->m_sortedMembers.end(), [&sortSpecs, &provider](Pattern *left, Pattern *right) {
                return Pattern::sortPatternTable(sortSpecs, provider, left, right);
            });

            for (auto &member : this->m_members)
                member->sort(sortSpecs, provider);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "struct " + Pattern::getTypeName();
        }

        [[nodiscard]] const auto &getMembers() const {
            return this->m_members;
        }

        void setMembers(std::vector<std::shared_ptr<Pattern>> &&members) {
            this->m_members.clear();

            for (auto &member : members) {
                if (member == nullptr) continue;

                this->m_sortedMembers.push_back(member.get());
                this->m_members.push_back(std::move(member));
            }
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherStruct = *static_cast<const PatternStruct *>(&other);
            if (this->m_members.size() != otherStruct.m_members.size())
                return false;

            for (u64 i = 0; i < this->m_members.size(); i++) {
                if (*this->m_members[i] != *otherStruct.m_members[i])
                    return false;
            }

            return true;
        }

        [[nodiscard]] const Pattern *getPattern(u64 offset) const override {
            if (this->isHidden()) return nullptr;

            auto iter = std::find_if(this->m_members.begin(), this->m_members.end(), [offset](const std::shared_ptr<Pattern> &pattern) {
                return offset >= pattern->getOffset() && offset < (pattern->getOffset() + pattern->getSize());
            });

            if (iter == this->m_members.end())
                return nullptr;
            else
                return (*iter)->getPattern(offset);
        }

        void setEndian(std::endian endian) override {
            for (auto &member : this->m_members) {
                if (!member->hasOverriddenEndian())
                    member->setEndian(endian);
            }

            Pattern::setEndian(endian);
        }

    private:
        std::vector<std::shared_ptr<Pattern>> m_members;
        std::vector<Pattern *> m_sortedMembers;
    };

}