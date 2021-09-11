#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternExample : public TestPattern {
    public:
        TestPatternExample() {

        }
        ~TestPatternExample() override = default;

        [[nodiscard]]
        std::string getSourceCode() const override {
            return R"(

            )";
        }

    };

}