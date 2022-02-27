#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

namespace hex::pl {

    class PatternSigned : public Pattern {
    public:
        PatternSigned(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternSigned(*this));
        }

        void createEntry(prv::Provider *&provider) override {
            i128 data = 0;
            provider->read(this->getOffset(), &data, this->getSize());
            data = hex::changeEndianess(data, this->getSize(), this->getEndian());

            data = hex::signExtend(this->getSize() * 8, data);
            this->createDefaultEntry(hex::format("{:d} (0x{:0{}X})", data, data, 1 * 2), data);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            switch (this->getSize()) {
                case 1:
                    return "s8";
                case 2:
                    return "s16";
                case 4:
                    return "s32";
                case 8:
                    return "s64";
                case 16:
                    return "s128";
                default:
                    return "Signed data";
            }
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }
    };

}