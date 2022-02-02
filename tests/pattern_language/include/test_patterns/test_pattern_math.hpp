#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternMath : public TestPattern {
    public:
        TestPatternMath() : TestPattern("Math") {
        }
        ~TestPatternMath() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                // Compare operations
                std::assert(123 == 123, "== operation error");
                std::assert(123 != 567, "!= operation error");
                std::assert(111 < 222, "< operation error");
                std::assert(333 > 222, "> operation error");
                std::assert(100 >= 100, ">= operation error");
                std::assert(200 <= 200, "<= operation error");

                // Boolean operations
                std::assert(true, "true literal invalid");
                std::assert(true && true, "&& operator error");
                std::assert(false || true, "|| operator error");
                std::assert(true ^^ false, "^^ operator error");
                std::assert(!false, "! operator error");

                // Bitwise operations
                std::assert(0xFF00FF | 0x00AA00 == 0xFFAAFF, "| operator error");
                std::assert(0xFFFFFF & 0x00FF00 == 0x00FF00, "& operator error");
                std::assert(0xFFFFFF ^ 0x00AA00 == 0xFF55FF, "^ operator error");
                std::assert(~0x00U == 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF, "~ operator error");
                std::assert(0xAA >> 4 == 0x0A, ">> operator error");
                std::assert(0xAA << 4 == 0xAA0, "<< operator error");

                // Basic operations
                std::assert(100 + 200 == 300, "+ operator error");
                std::assert(400 - 200 == 200, "- operator error");
                std::assert(10 * 20 == 200, "* operator error");
                std::assert(200 / 100 == 2, "/ operator error");
                std::assert(100 % 2 == 0, "% operator error");

                // Special operators
                std::assert($ == 0, "$ operator error");
                std::assert(((10 == 20) ? 30 : 40) == 40, "?: operator error");

                // Type operators
                struct TypeTest { u32 x, y, z; };
                TypeTest typeTest @ 0x100;

                std::assert(addressof(typeTest) == 0x100, "addressof operator error");
                std::assert(sizeof(typeTest) == 3 * 4, "sizeof operator error");

                // Properties
                std::assert(100 + 200 == 200 + 100, "+ operator commutativity error");
                std::assert(100 - 200 != 200 - 100, "- operator commutativity error");
                std::assert(100 * 200 == 200 * 100, "* operator commutativity error");
                std::assert(100F / 200F != 200F / 100F, "/ operator commutativity error");

                std::assert(10 + (20 + 30) == (10 + 20) + 30, "+ operator associativity error");
                std::assert(10 - (20 - 30) != (10 - 20) - 30, "- operator associativity error");
                std::assert(10 * (20 * 30) == (10 * 20) * 30, "* operator associativity error");
                std::assert(10F / (20F / 30F) != (10F / 20F) / 30F, "/ operator associativity error");

                std::assert(10 * (20 + 30) == 10 * 20 + 10 * 30, "* operator distributivity error");
                std::assert(10F / (20F + 30F) != 10F / 20F + 10F / 30F, "/ operator distributivity error");
            )";
        }
    };

}