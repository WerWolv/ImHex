#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternEnums : public TestPattern {
    public:
        TestPatternEnums() : TestPattern("Enums"){
            auto testEnum = create<PatternDataEnum>("TestEnum", "testEnum", 0x120, sizeof(u32));
            testEnum->setEnumValues({
                { u128(0x0000), "A" },
                { s128(0x1234), "B" },
                { u128(0x1235), "C" },
                { u128(0x1236), "D" },
            });

            addPattern(testEnum);
        }
        ~TestPatternEnums() override = default;

        [[nodiscard]]
        std::string getSourceCode() const override {
            return R"(
                enum TestEnum : u32 {
                    A,
                    B = 0x1234,
                    C,
                    D
                };

                TestEnum testEnum @ 0x120;
            )";
        }

    };

}