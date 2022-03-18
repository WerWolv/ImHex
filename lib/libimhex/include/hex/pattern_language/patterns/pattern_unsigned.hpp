#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

namespace hex::pl {

    class PatternUnsigned : public Pattern {
    public:
        PatternUnsigned(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternUnsigned(*this));
        }

        u128 getValue(prv::Provider *&provider) {
            u128 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());
            return hex::changeEndianess(data, this->getSize(), this->getEndian());
        }

        void createEntry(prv::Provider *&provider) override {
            u128 data = this->getValue(provider);
            this->createDefaultEntry(hex::format("{:d} (0x{:0{}X})", data, data, this->getSize() * 2), data);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            switch (this->getSize()) {
                case 1:
                    return "u8";
                case 2:
                    return "u16";
                case 4:
                    return "u32";
                case 8:
                    return "u64";
                case 16:
                    return "u128";
                default:
                    return "Unsigned data";
            }
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }
    };

}