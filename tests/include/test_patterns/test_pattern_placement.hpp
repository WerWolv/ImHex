#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternPlacement : public TestPattern {
    public:
        TestPatternPlacement() : TestPattern("Placement")  {
            // placementVar
            {
                addPattern(create<PatternDataUnsigned>(0x00, sizeof(u32), "u32", "placementVar"));
            }

            // placementArray
            {
                auto placementArray = create<PatternDataStaticArray>(0x10, sizeof(u8) * 10, "u8", "placementArray");
                placementArray->setEntries(create<PatternDataUnsigned>(0x10, sizeof(u8), "u8", ""), 10);
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