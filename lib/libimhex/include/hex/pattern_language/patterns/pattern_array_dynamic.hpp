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

        void getHighlightedAddresses(std::map<u64, u32> &highlight) const override {
            for (auto &entry : this->m_entries) {
                entry->getHighlightedAddresses(highlight);
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return this->m_entries[0]->getTypeName() + "[" + std::to_string(this->m_entries.size()) + "]";
        }

        [[nodiscard]] std::string getTypeName() const {
            return this->m_entries[0]->getTypeName();
        }

        void setOffset(u64 offset) override {
            for (auto &entry : this->m_entries)
                entry->setOffset(entry->getOffset() - this->getOffset() + offset);

            Pattern::setOffset(offset);
        }

        [[nodiscard]] size_t getEntryCount() const {
            return this->m_entries.size();
        }

        [[nodiscard]] const auto &getEntries() {
            return this->m_entries;
        }

        void forEachArrayEntry(const std::function<void(int, Pattern&)>& fn) {
            for (u64 i = 0; i < this->m_entries.size(); i++)
                fn(i, *this->m_entries[i]);
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
        std::vector<std::shared_ptr<Pattern>> m_entries;
        u64 m_displayEnd = 50;
    };

}