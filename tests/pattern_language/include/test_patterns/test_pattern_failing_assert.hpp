#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternFailingAssert : public TestPattern {
    public:
        TestPatternFailingAssert() : TestPattern("FailingAssert", Mode::Failing) {
        }
        ~TestPatternFailingAssert() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                #define MSG "Error"

                std::assert(false, MSG);

            )";
        }
    };

}