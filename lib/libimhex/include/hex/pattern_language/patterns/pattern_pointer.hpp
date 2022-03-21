#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

namespace hex::pl {

    class PatternPointer : public Pattern,
                           public Inlinable {
    public:
        PatternPointer(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color), m_pointedAt(nullptr) {
        }

        PatternPointer(const PatternPointer &other) : Pattern(other) {
            this->m_pointedAt = other.m_pointedAt->clone();
        }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternPointer(*this));
        }

        u64 getValue(prv::Provider *&provider) {
            u64 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());
            return hex::changeEndianess(data, this->getSize(), this->getEndian());
        }

        void getHighlightedAddresses(std::map<u64, u32> &highlight) const override {
            Pattern::getHighlightedAddresses(highlight);
            this->m_pointedAt->getHighlightedAddresses(highlight);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            std::string result = this->getTypeName().empty() ? this->getFormattedName() : this->getTypeName() + "* : ";
            switch (this->getSize()) {
                case 1:
                    result += "u8";
                    break;
                case 2:
                    result += "u16";
                    break;
                case 4:
                    result += "u32";
                    break;
                case 8:
                    result += "u64";
                    break;
                case 16:
                    result += "u128";
                    break;
            }

            return result;
        }

        void setPointedAtPattern(std::unique_ptr<Pattern> &&pattern) {
            this->m_pointedAt = std::move(pattern);
            this->m_pointedAt->setVariableName(hex::format("*({})", this->getVariableName()));
            this->m_pointedAt->setOffset(this->m_pointedAtAddress);
        }

        void setPointedAtAddress(u64 address) {
            this->m_pointedAtAddress = address;
        }

        [[nodiscard]] u64 getPointedAtAddress() const {
            return this->m_pointedAtAddress;
        }

        [[nodiscard]] const std::unique_ptr<Pattern> &getPointedAtPattern() {
            return this->m_pointedAt;
        }

        void setColor(u32 color) override {
            Pattern::setColor(color);
            this->m_pointedAt->setColor(color);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override {
            return areCommonPropertiesEqual<decltype(*this)>(other) &&
                   *static_cast<const PatternPointer *>(&other)->m_pointedAt == *this->m_pointedAt;
        }

        void rebase(u64 base) {
            if (this->m_pointedAt != nullptr) {
                this->m_pointedAtAddress = (this->m_pointedAt->getOffset() - this->m_pointerBase) + base;
                this->m_pointedAt->setOffset(this->m_pointedAtAddress);
            }

            this->m_pointerBase = base;
        }

        [[nodiscard]] const Pattern *getPattern(u64 offset) const override {
            if (offset >= this->getOffset() && offset < (this->getOffset() + this->getSize()) && !this->isHidden())
                return this;
            else
                return this->m_pointedAt->getPattern(offset);
        }

        void setEndian(std::endian endian) override {
            this->m_pointedAt->setEndian(endian);

            Pattern::setEndian(endian);
        }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }

    private:
        std::unique_ptr<Pattern> m_pointedAt;
        u64 m_pointedAtAddress = 0;

        u64 m_pointerBase = 0;
    };

}