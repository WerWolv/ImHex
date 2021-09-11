#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternUnions : public TestPattern {
    public:
        TestPatternUnions() : TestPattern("Unions")  {
            auto testUnion = create<PatternDataUnion>(0x200, sizeof(u128), "TestUnion", "testUnion");

            auto array = create<PatternDataStaticArray>(0x200, sizeof(s32[2]), "s32", "array");
            array->setEntries(create<PatternDataSigned>(0x200, sizeof(s32), "s32", ""), 2);
            auto variable = create<PatternDataUnsigned>(0x200, sizeof(u128), "u128", "variable");

            testUnion->setMembers({ array, variable });

            addPattern(testUnion);
        }
        ~TestPatternUnions() override = default;

        [[nodiscard]]
        std::string getSourceCode() const override {
            return R"(
                union TestUnion {
                    s32 array[2];
                    u128 variable;
                };

                TestUnion testUnion @ 0x200;
            )";
        }

    };

}