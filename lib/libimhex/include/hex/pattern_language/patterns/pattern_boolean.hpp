#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

namespace hex::pl {

    class PatternBoolean : public Pattern {
    public:
        explicit PatternBoolean(Evaluator *evaluator, u64 offset, u32 color = 0)
            : Pattern(evaluator, offset, 1, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternBoolean(*this));
        }

        u8 getValue(prv::Provider *&provider) {
            u8 boolean;
            provider->read(this->getOffset(), &boolean, 1);
            return boolean;
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bool";
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }
    };

}