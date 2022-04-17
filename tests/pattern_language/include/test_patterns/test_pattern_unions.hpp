#pragma once

#include "test_pattern.hpp"

#include <hex/pattern_language/patterns/pattern_unsigned.hpp>
#include <hex/pattern_language/patterns/pattern_signed.hpp>
#include <hex/pattern_language/patterns/pattern_array_static.hpp>
#include <hex/pattern_language/patterns/pattern_union.hpp>

namespace hex::test {

    class TestPatternUnions : public TestPattern {
    public:
        TestPatternUnions() : TestPattern("Unions") {
            auto testUnion = create<PatternUnion>("TestUnion", "testUnion", 0x200, sizeof(u128));

            auto array = create<PatternArrayStatic>("s32", "array", 0x200, sizeof(i32[2]));
            array->setEntries(create<PatternSigned>("s32", "", 0x200, sizeof(i32)), 2);
            auto variable = create<PatternUnsigned>("u128", "variable", 0x200, sizeof(u128));

            std::vector<std::shared_ptr<pl::Pattern>> unionMembers;
            {
                unionMembers.push_back(std::move(array));
                unionMembers.push_back(std::move(variable));
            }

            testUnion->setMembers(std::move(unionMembers));

            addPattern(std::move(testUnion));
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