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

        u64 getValue(prv::Provider *&provider) {
            std::vector<u8> value(this->m_bitField->getSize(), 0);
            provider->read(this->m_bitField->getOffset(), &value[0], value.size());

            if (this->m_bitField->getEndian() != std::endian::native)
                std::reverse(value.begin(), value.end());

            u8 numBytes = (this->m_bitSize / 8) + 1;

            return hex::extract(this->m_bitOffset + (this->m_bitSize - 1), this->m_bitOffset, value);
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

        std::vector<u8> getValue(prv::Provider *&provider) const {
            std::vector<u8> value(this->getSize(), 0);
            provider->read(this->getOffset(), &value[0], value.size());

            if (this->getEndian() == std::endian::little)
                std::reverse(value.begin(), value.end());

            return value;
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