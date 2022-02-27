#pragma once

#include "test_pattern.hpp"

#include <hex/pattern_language/patterns/pattern_unsigned.hpp>
#include <hex/pattern_language/patterns/pattern_pointer.hpp>

namespace hex::test {

    class TestPatternPointers : public TestPattern {
    public:
        TestPatternPointers() : TestPattern("Pointers") {
            // placementPointer
            {
                auto placementPointer = create<PatternPointer>("", "placementPointer", 0x0C, sizeof(u8));
                placementPointer->setPointedAtAddress(0x49);

                auto pointedTo = create<PatternUnsigned>("u32", "", 0x49, sizeof(u32));
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