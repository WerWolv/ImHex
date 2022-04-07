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

        void forEachArrayEntry(const std::function<void(u64, Pattern&)>& fn) {
            auto entry = std::shared_ptr(this->m_template->clone());
            for (u64 index = 0; index < this->m_entryCount; index++) {
                entry->clearFormatCache();
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

        [[nodiscard]] std::string getTypeName() const override {
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

    private:
        std::shared_ptr<Pattern> m_template                  = nullptr;
        mutable std::unique_ptr<Pattern> m_highlightTemplate = nullptr;
        size_t m_entryCount                                  = 0;
    };

}