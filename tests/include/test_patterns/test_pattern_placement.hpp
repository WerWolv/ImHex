#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternPlacement : public TestPattern {
    public:
        TestPatternPlacement() {
            // placementVar
            {
                addPattern(createVariablePattern<pl::PatternDataUnsigned>(0x00, sizeof(u32), "u32", "placementVar"));
            }

            // placementArray
            {
                auto placementArray = createVariablePattern<pl::PatternDataStaticArray>(0x10, sizeof(u8) * 10, "u8", "placementArray");
                placementArray->setEntries(createVariablePattern<pl::PatternDataUnsigned>(0x10, sizeof(u8), "u8", ""), 10);
                addPattern(placementArray);
            }

        }
        ~TestPatternPlacement() override = default;

        [[nodiscard]]
        std::string getSourceCode() const override {
            return R"(
                u32 placementVar @ 0x00;
                u8 placementArray[10] @ 0x10;
            )";
        }

    };

}