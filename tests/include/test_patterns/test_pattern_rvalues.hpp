#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternRValues : public TestPattern {
    public:
        TestPatternRValues() : TestPattern("RValues")  {

        }
        ~TestPatternRValues() override = default;

        [[nodiscard]]
        std::string getSourceCode() const override {
            return R"(
                union C {
                    u8 y;
                    u8 array[parent.parent.x];
                };

                struct B {
                    C *c : u8;
                };

                struct A {
                    u8 x;
                    B b;
                };

                A a @ 0x00;

                std::assert(sizeof(a.b.c) == a.x && a.x != 0x00, "RValue parent test failed!");
                std::assert(a.b.c.y == a.b.c.array[0], "RValue array access test failed!");
            )";
        }

    };

}