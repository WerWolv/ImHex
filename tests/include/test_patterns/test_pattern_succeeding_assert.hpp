#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternSucceedingAssert : public TestPattern {
    public:
        TestPatternSucceedingAssert() : TestPattern("SucceedingAssert")  {

        }
        ~TestPatternSucceedingAssert() override = default;

        [[nodiscard]]
        std::string getSourceCode() const override {
            return R"(
                #define MSG "Error"

                std::assert(true, MSG);
                std::assert(100 == 100, MSG);
                std::assert(50 < 100, MSG);
                std::assert(1, MSG);

            )";
        }

    };

}