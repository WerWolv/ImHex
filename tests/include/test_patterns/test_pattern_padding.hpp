#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternPadding : public TestPattern {
    public:
        TestPatternPadding() : TestPattern("Padding")  {
            auto testStruct = create<PatternDataStruct>(0x100, sizeof(s32) + 20 + sizeof(u8[0x10]), "TestStruct", "testStruct");

            auto variable = create<PatternDataSigned>(0x100, sizeof(s32), "s32", "variable");
            auto padding = create<PatternDataPadding>(0x100 + sizeof(s32), 20, "", "");
            auto array = create<PatternDataStaticArray>(0x100 + sizeof(s32) + 20, sizeof(u8[0x10]), "u8", "array");
            array->setEntries(create<PatternDataUnsigned>(0x100 + sizeof(s32) + 20, sizeof(u8), "u8", ""), 0x10);

            testStruct->setMembers({ variable, padding, array });

            addPattern(testStruct);
        }
        ~TestPatternPadding() override = default;

        [[nodiscard]]
        std::string getSourceCode() const override {
            return R"(
                struct TestStruct {
                    s32 variable;
                    padding[20];
                    u8 array[0x10];
                };

                TestStruct testStruct @ 0x100;
            )";
        }

    };

}