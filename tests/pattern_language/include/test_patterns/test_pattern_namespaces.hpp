#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternNamespaces : public TestPattern {
    public:
        TestPatternNamespaces() : TestPattern("Namespaces") {
        }
        ~TestPatternNamespaces() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                namespace A {
                    struct Test {
                        u32 x;
                    };
                }

                namespace B {
                    struct Test {
                        u16 x;
                    };
                }

                using ATest = A::Test;

                A::Test test1 @ 0x10;
                ATest test2 @ 0x20;
                B::Test test3 @ 0x20;

                std::assert(sizeof(test1) == sizeof(test2), "error using namespaced type");
                std::assert(sizeof(test2) != sizeof(test3), "error differentiating two namespace types with same name");
            )";
        }
    };

}