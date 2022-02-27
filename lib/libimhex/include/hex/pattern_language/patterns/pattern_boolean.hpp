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

        void createEntry(prv::Provider *&provider) override {
            u8 boolean;
            provider->read(this->getOffset(), &boolean, 1);

            if (boolean == 0)
                this->createDefaultEntry("false", false);
            else if (boolean == 1)
                this->createDefaultEntry("true", true);
            else
                this->createDefaultEntry("true*", true);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "bool";
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }
    };

}