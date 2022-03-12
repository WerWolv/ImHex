#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

namespace hex::pl {

    class PatternArrayStatic : public Pattern,
                               public Inlinable {
    public:
        PatternArrayStatic(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) {
        }

        PatternArrayStatic(const PatternArrayStatic &other) : Pattern(other) {
            this->setEntries(other.getTemplate()->clone(), other.getEntryCount());
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternArrayStatic(*this));
        }

        void createEntry(prv::Provider *&provider) override {
            if (this->getEntryCount() == 0)
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
                ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{0}", this->m_template->getTypeName().c_str());
                ImGui::SameLine(0, 0);

                ImGui::TextUnformatted("[");
                ImGui::SameLine(0, 0);
                ImGui::TextFormattedColored(ImColor(0xFF00FF00), "{0}", this->m_entryCount);
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted("]");

                ImGui::TableNextColumn();
                ImGui::TextFormatted("{}", this->formatDisplayValue("{ ... }", this));
            } else {
                ImGui::SameLine();
                ImGui::TreeNodeEx("", ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf);
            }

            if (open) {
                auto entry = this->m_template->clone();
                for (u64 index = 0; index < this->m_entryCount; index++) {
                    entry->clearFormatCache();
                    entry->setVariableName(hex::format("[{0}]", index));
                    entry->setOffset(this->getOffset() + index * this->m_template->getSize());
                    entry->draw(provider);

                    if (index >= (this->m_displayEnd - 1)) {
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

        void forEachArrayEntry(const std::function<void(int, Pattern&)>& fn) {
            auto entry = std::shared_ptr(this->m_template->clone());
            for (u64 index = 0; index < this->m_entryCount; index++) {
                entry->setVariableName(hex::format("[{0}]", index));
                entry->setOffset(this->getOffset() + index * this->m_template->getSize());
                fn(index, *entry);
            }
        }

        void getHighlightedAddresses(std::map<u64, u32> &highlight) const override {
            auto entry = this->m_template->clone();

            for (u64 address = this->getOffset(); address < this->getOffset() + this->getSize(); address += entry->getSize()) {
                entry->setOffset(address);
                entry->getHighlightedAddresses(highlight);
            }
        }

        void setOffset(u64 offset) override {
            this->m_template->setOffset(this->m_template->getOffset() - this->getOffset() + offset);

            Pattern::setOffset(offset);
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            this->m_template->setColor(color);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->m_template->getTypeName() + "[" + std::to_string(this->m_entryCount) + "]";
        }

        [[nodiscard]] std::string getTypeName() const {
            return this->m_template->getTypeName();
        }

        [[nodiscard]] const std::shared_ptr<Pattern> &getTemplate() const {
            return this->m_template;
        }

        [[nodiscard]] size_t getEntryCount() const {
            return this->m_entryCount;
        }

        void setEntryCount(size_t count) {
            this->m_entryCount = count;
        }

        void setEntries(std::unique_ptr<Pattern> &&templatePattern, size_t count) {
            this->m_template          = std::move(templatePattern);
            this->m_highlightTemplate = this->m_template->clone();
            this->m_entryCount        = count;

            this->m_template->setBaseColor(this->getColor());
            this->m_highlightTemplate->setBaseColor(this->getColor());
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            if (!areCommonPropertiesEqual<decltype(*this)>(other))
                return false;

            auto &otherArray = *static_cast<const PatternArrayStatic *>(&other);
            return *this->m_template == *otherArray.m_template && this->m_entryCount == otherArray.m_entryCount;
        }

        [[nodiscard]] const Pattern *getPattern(u64 offset) const override {
            if (this->isHidden()) return nullptr;

            this->m_highlightTemplate->setBaseColor(this->getColor());
            this->m_highlightTemplate->setVariableName(this->getVariableName());
            this->m_highlightTemplate->setDisplayName(this->getDisplayName());

            if (offset >= this->getOffset() && offset < (this->getOffset() + this->getSize())) {
                this->m_highlightTemplate->setOffset((offset / this->m_highlightTemplate->getSize()) * this->m_highlightTemplate->getSize());
                return this->m_highlightTemplate->getPattern(offset);
            } else {
                return nullptr;
            }
        }

        void setEndian(std::endian endian) override {
            this->m_template->setEndian(endian);

            Pattern::setEndian(endian);
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

        u64 getDisplayEnd() const {
            return m_displayEnd;
        }

        void resetDisplayEnd() {
            m_displayEnd = 50;
        }

        void increaseDisplayEnd() {
            m_displayEnd += 50;
        }

    private:
        std::shared_ptr<Pattern> m_template                  = nullptr;
        mutable std::unique_ptr<Pattern> m_highlightTemplate = nullptr;
        size_t m_entryCount                                  = 0;
        u64 m_displayEnd                                     = 50;
    };

}