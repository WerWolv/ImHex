#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

namespace hex::pl {

    class PatternPadding : public Pattern {
    public:
        PatternPadding(Evaluator *evaluator, u64 offset, size_t size) : Pattern(evaluator, offset, size, 0xFF000000) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternPadding(*this));
        }

        void createEntry(prv::Provider *&provider) override {
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "";
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }
    };

}