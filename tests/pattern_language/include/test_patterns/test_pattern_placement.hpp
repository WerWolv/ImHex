#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternPlacement : public TestPattern {
    public:
        TestPatternPlacement() : TestPattern("Placement") {
            // placementVar
            {
                addPattern(create<PatternDataUnsigned>("u32", "placementVar", 0x00, sizeof(u32)));
            }

            // placementArray
            {
                auto placementArray = create<PatternDataStaticArray>("u8", "placementArray", 0x10, sizeof(u8) * 10);
                placementArray->setEntries(std::move(create<PatternDataUnsigned>("u8", "", 0x10, sizeof(u8))), 10);
                addPattern(std::move(placementArray));
            }
        }
        ~TestPatternPlacement() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                u32 placementVar @ 0x00;
                u8 placementArray[10] @ 0x10;
            )";
        }
    };

}