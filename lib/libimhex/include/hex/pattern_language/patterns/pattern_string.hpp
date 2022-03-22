#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

namespace hex::pl {

    class PatternString : public Pattern {
    public:
        PatternString(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternString(*this));
        }

        std::string getValue(prv::Provider *&provider) const {
            return this->getValue(provider, this->getSize());
        }

        std::string getValue(prv::Provider *&provider, size_t size) const {
            std::vector<u8> buffer(size, 0x00);
            provider->read(this->getOffset(), buffer.data(), size);
            return hex::encodeByteString(buffer);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "String";
        }

        [[nodiscard]] std::string toString(prv::Provider *provider) const override {
            return this->getValue(provider);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }
    };

}