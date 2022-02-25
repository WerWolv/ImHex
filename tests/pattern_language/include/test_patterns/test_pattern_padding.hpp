#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternPadding : public TestPattern {
    public:
        TestPatternPadding() : TestPattern("Padding") {
            auto testStruct = create<PatternDataStruct>("TestStruct", "testStruct", 0x100, sizeof(i32) + 20 + sizeof(u8[0x10]));

            auto variable = create<PatternDataSigned>("s32", "variable", 0x100, sizeof(i32));
            auto padding  = create<PatternDataPadding>("padding", "", 0x100 + sizeof(i32), 20);
            auto array    = create<PatternDataStaticArray>("u8", "array", 0x100 + sizeof(i32) + 20, sizeof(u8[0x10]));
            array->setEntries(create<PatternDataUnsigned>("u8", "", 0x100 + sizeof(i32) + 20, sizeof(u8)), 0x10);

            std::vector<std::shared_ptr<hex::pl::PatternData>> structMembers;
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