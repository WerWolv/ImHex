#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternPointers : public TestPattern {
    public:
        TestPatternPointers() : TestPattern("Pointers") {
            // placementPointer
            {
                auto placementPointer = create<PatternDataPointer>("", "placementPointer", 0x0C, sizeof(u8));
                placementPointer->setPointedAtAddress(0x49);

                auto pointedTo = create<PatternDataUnsigned>("u32", "", 0x49, sizeof(u32));
                placementPointer->setPointedAtPattern(std::move(pointedTo));
                addPattern(std::move(placementPointer));
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