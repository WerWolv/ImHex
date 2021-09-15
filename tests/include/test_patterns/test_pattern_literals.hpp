#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternLiterals : public TestPattern {
    public:
        TestPatternLiterals() : TestPattern("Literals")  {

        }
        ~TestPatternLiterals() override = default;

        [[nodiscard]]
        std::string getSourceCode() const override {
            return R"(
                #define MSG "Invalid literal"

                std::assert(255 == 0xFF, MSG);
                std::assert(0xAA == 0b10101010, MSG);
                std::assert(12345 != 67890, MSG);
                std::assert(100U == 0x64U, MSG);
                std::assert(-100 == -0x64, MSG);
                std::assert(3.14159F > 1.414D, MSG);
                std::assert('A' == 0x41, MSG);

            )";
        }

    };

}