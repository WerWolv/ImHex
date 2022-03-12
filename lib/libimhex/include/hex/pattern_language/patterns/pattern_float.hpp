#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

namespace hex::pl {

    class PatternFloat : public Pattern {
    public:
        PatternFloat(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternFloat(*this));
        }

        void createEntry(prv::Provider *&provider) override {
            if (this->getSize() == 4) {
                u32 data = 0;
                provider->read(this->getOffset(), &data, 4);
                data = hex::changeEndianess(data, 4, this->getEndian());

                this->createDefaultEntry(hex::format("{:e} (0x{:0{}X})", *reinterpret_cast<float *>(&data), data, this->getSize() * 2), *reinterpret_cast<float *>(&data));
            } else if (this->getSize() == 8) {
                u64 data = 0;
                provider->read(this->getOffset(), &data, 8);
                data = hex::changeEndianess(data, 8, this->getEndian());

                this->createDefaultEntry(hex::format("{:e} (0x{:0{}X})", *reinterpret_cast<double *>(&data), data, this->getSize() * 2), *reinterpret_cast<double *>(&data));
            }
        }

        [[nodiscard]] std::string getFormattedName() const override {
            switch (this->getSize()) {
                case 4:
                    return "float";
                case 8:
                    return "double";
                default:
                    return "Floating point data";
            }
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }
    };

}