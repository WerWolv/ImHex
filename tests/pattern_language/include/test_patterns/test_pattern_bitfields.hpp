#pragma once

#include "test_pattern.hpp"

namespace hex::test {

    class TestPatternBitfields : public TestPattern {
    public:
        TestPatternBitfields() : TestPattern("Bitfields")  {
            auto testBitfield = create<PatternDataBitfield>("TestBitfield", "testBitfield", 0x12, (4 * 4) / 8, nullptr);
            testBitfield->setEndian(std::endian::big);
            testBitfield->setFields({
                    create<PatternDataBitfieldField>("", "a", 0x12, 0, 4, nullptr),
                    create<PatternDataBitfieldField>("", "b", 0x12, 4, 4, nullptr),
                    create<PatternDataBitfieldField>("", "c", 0x12, 8, 4, nullptr),
                    create<PatternDataBitfieldField>("", "d", 0x12, 12, 4, nullptr)
            });

            addPattern(testBitfield);
        }
        ~TestPatternBitfields() override = default;

        [[nodiscard]]
        std::string getSourceCode() const override {
            return R"(
                bitfield TestBitfield {
                    a : 4;
                    b : 4;
                    c : 4;
                    d : 4;
                };

                be TestBitfield testBitfield @ 0x12;

                std::assert(testBitfield.a == 0x0A, "Field A invalid");
                std::assert(testBitfield.b == 0x00, "Field B invalid");
                std::assert(testBitfield.c == 0x04, "Field C invalid");
                std::assert(testBitfield.d == 0x03, "Field D invalid");
            )";
        }

    };

}