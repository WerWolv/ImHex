#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternExample : public TestPattern {
    public:
        TestPatternExample() {
            auto placementTest = new pl::PatternDataSigned(0x00, sizeof(u32));
            placementTest->setTypeName("u32");
            placementTest->setVariableName("placementTest");
            addPattern(placementTest);
        }
        ~TestPatternExample() override = default;

        [[nodiscard]]
        std::string getSourceCode() const override {
            return R"(
                u32 placementTest @ 0x00;
            )";
        }

    };

}