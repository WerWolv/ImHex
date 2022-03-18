#pragma once

#include <hex/pattern_language/patterns/pattern.hpp>

#include <codecvt>

namespace hex::pl {

    class PatternWideString : public Pattern {
    public:
        PatternWideString(Evaluator *evaluator, u64 offset, size_t size, u32 color = 0)
            : Pattern(evaluator, offset, size, color) { }

        [[nodiscard]] std::unique_ptr<Pattern> clone() const override {
            return std::unique_ptr<Pattern>(new PatternWideString(*this));
        }

        std::string getValue(prv::Provider *&provider) {
            return this->getValue(provider, this->getSize());
        }

        std::string getValue(prv::Provider *&provider, size_t size) {
            std::u16string buffer(this->getSize() / sizeof(char16_t), 0x00);
            provider->read(this->getOffset(), buffer.data(), size);

            for (auto &c : buffer)
                c = hex::changeEndianess(c, 2, this->getEndian());

            auto it = std::remove_if(buffer.begin(), buffer.end(),
                                     [](auto c) { return c == 0x00; });
            buffer.erase(it, buffer.end());

            return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> {}.to_bytes(buffer);
        }

        [[nodiscard]] std::string getFormattedName() const override {
            return "String16";
        }

        [[nodiscard]] std::string toString(prv::Provider *provider) const override {
            std::u16string buffer(this->getSize() / sizeof(char16_t), 0x00);
            provider->read(this->getOffset(), buffer.data(), this->getSize());

            for (auto &c : buffer)
                c = hex::changeEndianess(c, 2, this->getEndian());

            std::erase_if(buffer, [](auto c) {
                return c == 0x00;
            });

            return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> {}.to_bytes(buffer);
        }

        [[nodiscard]] bool operator==(const Pattern &other) const override { return areCommonPropertiesEqual<decltype(*this)>(other); }

        void accept(PatternVisitor &v) override {
            v.visit(*this);
        }
    };

}