#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternUnions : public TestPattern {
    public:
        TestPatternUnions() : TestPattern("Unions") {
            auto testUnion = create<PatternDataUnion>("TestUnion", "testUnion", 0x200, sizeof(u128));

            auto array = create<PatternDataStaticArray>("s32", "array", 0x200, sizeof(i32[2]));
            array->setEntries(create<PatternDataSigned>("s32", "", 0x200, sizeof(i32)), 2);
            auto variable = create<PatternDataUnsigned>("u128", "variable", 0x200, sizeof(u128));

            testUnion->setMembers({ array, variable });

            addPattern(testUnion);
        }
        ~TestPatternUnions() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
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