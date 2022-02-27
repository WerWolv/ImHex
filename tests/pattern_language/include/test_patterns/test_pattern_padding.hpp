#pragma once

#include "test_pattern.hpp"

#include <hex/pattern_language/patterns/pattern_unsigned.hpp>
#include <hex/pattern_language/patterns/pattern_signed.hpp>
#include <hex/pattern_language/patterns/pattern_struct.hpp>
#include <hex/pattern_language/patterns/pattern_padding.hpp>
#include <hex/pattern_language/patterns/pattern_array_static.hpp>

namespace hex::test {

    class TestPatternPadding : public TestPattern {
    public:
        TestPatternPadding() : TestPattern("Padding") {
            auto testStruct = create<PatternStruct>("TestStruct", "testStruct", 0x100, sizeof(i32) + 20 + sizeof(u8[0x10]));

            auto variable = create<PatternSigned>("s32", "variable", 0x100, sizeof(i32));
            auto padding  = create<PatternPadding>("padding", "", 0x100 + sizeof(i32), 20);
            auto array    = create<PatternArrayStatic>("u8", "array", 0x100 + sizeof(i32) + 20, sizeof(u8[0x10]));
            array->setEntries(create<PatternUnsigned>("u8", "", 0x100 + sizeof(i32) + 20, sizeof(u8)), 0x10);

            std::vector<std::shared_ptr<hex::pl::Pattern>> structMembers;
            {
                structMembers.push_back(std::move(variable));
                structMembers.push_back(std::move(padding));
                structMembers.push_back(std::move(array));
            }

            testStruct->setMembers(std::move(structMembers));

            addPattern(std::move(testStruct));
        }
        ~TestPatternPadding() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
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