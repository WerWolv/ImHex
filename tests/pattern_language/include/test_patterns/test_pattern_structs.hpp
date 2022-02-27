#pragma once

#include "test_pattern.hpp"

#include <hex/pattern_language/patterns/pattern_unsigned.hpp>
#include <hex/pattern_language/patterns/pattern_signed.hpp>
#include <hex/pattern_language/patterns/pattern_struct.hpp>
#include <hex/pattern_language/patterns/pattern_array_static.hpp>

namespace hex::test {

    class TestPatternStructs : public TestPattern {
    public:
        TestPatternStructs() : TestPattern("Structs") {
            auto testStruct = create<PatternStruct>("TestStruct", "testStruct", 0x100, sizeof(i32) + sizeof(u8[0x10]));

            auto variable = create<PatternSigned>("s32", "variable", 0x100, sizeof(i32));
            auto array    = create<PatternArrayStatic>("u8", "array", 0x100 + sizeof(i32), sizeof(u8[0x10]));
            array->setEntries(create<PatternUnsigned>("u8", "", 0x100 + sizeof(i32), sizeof(u8)), 0x10);

            std::vector<std::shared_ptr<hex::pl::Pattern>> structMembers;
            {
                structMembers.push_back(std::move(variable));
                structMembers.push_back(std::move(array));
            }
            testStruct->setMembers(std::move(structMembers));

            addPattern(std::move(testStruct));
        }
        ~TestPatternStructs() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
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