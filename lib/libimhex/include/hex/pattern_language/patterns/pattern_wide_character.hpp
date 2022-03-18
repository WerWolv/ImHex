#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

#include <codecvt>

namespace hex::pl {

    class PatternWideCharacter : public Pattern {
    public:
        explicit PatternWideCharacter(Evaluator *evaluator, u64 offset, u32 color = 0)
            : Pattern(evaluator, offset, 2, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternWideCharacter(*this));
        }

        char16_t getValue(prv::Provider *&provider) {
            char16_t character;
            provider->read(this->getOffset(), &character, 2);
            return hex::changeEndianess(character, this->getEndian());
        }

        void createEntry(prv::Provider *&provider) override {
            char16_t character = this->getValue(provider);
            u128 literal = character;
            this->createDefaultEntry(hex::format("'{0}'", std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> {}.to_bytes(character)), literal);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "char16";
        }

        [[nodiscard]] std::string toString(prv::Provider *provider) const override {
            char16_t character;
            provider->read(this->getOffset(), &character, 2);
            character = hex::changeEndianess(character, this->getEndian());

            return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> {}.to_bytes(character);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }
    };

}