#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternStructs : public TestPattern {
    public:
        TestPatternStructs() : TestPattern("Structs")  {
            auto testStruct = create<PatternDataStruct>("TestStruct", "testStruct", 0x100, sizeof(s32) + sizeof(u8[0x10]));

            auto variable = create<PatternDataSigned>("s32", "variable", 0x100, sizeof(s32));
            auto array = create<PatternDataStaticArray>("u8", "array", 0x100 + sizeof(s32), sizeof(u8[0x10]));
            array->setEntries(create<PatternDataUnsigned>("u8", "", 0x100 + sizeof(s32), sizeof(u8)), 0x10);

            testStruct->setMembers({ variable, array });

            addPattern(testStruct);
        }
        ~TestPatternStructs() override = default;

        [[nodiscard]]
        std::string getSourceCode() const override {
            return R"(
                struct TestStruct {
                    s32 variable;
                    u8 array[0x10];
                };

                TestStruct testStruct @ 0x100;
            )";
        }

    };

}