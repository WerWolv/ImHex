#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternPointers : public TestPattern {
    public:
        TestPatternPointers() : TestPattern("Pointers") {
            // placementPointer
            {
                auto placementPointer = create<PatternDataPointer>("", "placementPointer", 0x0C, sizeof(u8), nullptr);
                placementPointer->setPointedAtAddress(0x49);

                auto pointedTo = create<PatternDataUnsigned>("u32", "", 0x49, sizeof(u32), nullptr);
                placementPointer->setPointedAtPattern(pointedTo);
                addPattern(placementPointer);
            }
        }
        ~TestPatternPointers() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                u32 *placementPointer : u8 @ 0x0C;
            )";
        }
    };

}