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

        double getValue(prv::Provider *&provider) {
            if (this->getSize() == 4) {
                u32 data = 0;
                provider->read(this->getOffset(), &data, 4);
                data = hex::changeEndianess(data, 4, this->getEndian());
                return *reinterpret_cast<float *>(&data);
            } else if (this->getSize() == 8) {
                u64 data = 0;
                provider->read(this->getOffset(), &data, 8);
                data = hex::changeEndianess(data, 8, this->getEndian());
                return *reinterpret_cast<double *>(&data);
            } else {
                assert(false);
                return std::numeric_limits<double>::quiet_NaN();
            }
        }

        void createEntry(prv::Provider *&provider) override {
            if (this->getSize() == 4) {
                float f32 = this->getValue(provider);
                u32 data = *reinterpret_cast<u32 *>(&f32);
                this->createDefaultEntry(hex::format("{:e} (0x{:0{}X})", f32, data, this->getSize() * 2), f32);
            } else if (this->getSize() == 8) {
                double f64 = this->getValue(provider);
                u64 data = *reinterpret_cast<u64 *>(&f64);
                this->createDefaultEntry(hex::format("{:e} (0x{:0{}X})", f64, data, this->getSize() * 2), f64);
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