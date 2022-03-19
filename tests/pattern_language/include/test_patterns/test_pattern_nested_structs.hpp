#pragma once

#include "test_pattern.hpp"

#include <hex/pattern_language/patterns/pattern_struct.hpp>
#include <hex/pattern_language/patterns/pattern_array_dynamic.hpp>
#include <hex/pattern_language/patterns/pattern_array_static.hpp>

#include <vector>

namespace hex::test {

    class TestPatternNestedStructs : public TestPattern {
    public:
        TestPatternNestedStructs() : TestPattern("NestedStructs") {
            const size_t HEADER_START = 0x0;
            const size_t HEADER_SIZE = sizeof(u8);
            const size_t BODY_START = HEADER_SIZE;
            const size_t BODY_SIZE = 0x89 - 1;

            auto data = create<PatternStruct>("Data", "data", HEADER_START, HEADER_SIZE + BODY_SIZE);
            {
                auto hdr = create<PatternStruct>("Header", "hdr", HEADER_START, HEADER_SIZE);
                {
                    std::vector<std::shared_ptr<hex::pl::Pattern>> hdrMembers {
                        std::shared_ptr(create<PatternUnsigned>("u8", "len", HEADER_START, sizeof(u8)))
                    };
                    hdr->setMembers(std::move(hdrMembers));
                }

                auto body = create<PatternStruct>("Body", "body", BODY_START, BODY_SIZE);
                {
                    auto bodyArray = create<PatternArrayStatic>("u8", "arr", BODY_START, BODY_SIZE);
                    bodyArray->setEntries(create<PatternUnsigned>("u8", "", BODY_START, sizeof(u8)), BODY_SIZE);
                    std::vector<std::shared_ptr<hex::pl::Pattern>> bodyMembers {
                        std::shared_ptr(std::move(bodyArray))
                    };
                    body->setMembers(std::move(bodyMembers));
                }

                std::vector<std::shared_ptr<hex::pl::Pattern>> dataMembers {
                    std::shared_ptr(std::move(hdr)),
                    std::shared_ptr(std::move(body))
                };
                data->setMembers(std::move(dataMembers));
            }

            addPattern(std::move(data));
        }
        ~TestPatternNestedStructs() override = default;

        [[nodiscard]] std::string getSourceCode() const override {
            return R"(
                fn end_of_body() {
                    u32 start = addressof(parent.parent.hdr);
                    u32 len = parent.parent.hdr.len;
                    u32 end = start + len;

                    return $ >= end;
                };

                struct Header {
                    u8 len;
                };

                struct Body {
                    u8 arr[while(!end_of_body())];
                };

                struct Data {
                    Header hdr;
                    Body body;
                };

                Data data @ 0x0;

                std::assert(data.hdr.len == 0x89, "Invalid length");
                std::assert(sizeof(data.body.arr) == 0x89 - 1, "Invalid size of body");
            )";
        }
    };

}