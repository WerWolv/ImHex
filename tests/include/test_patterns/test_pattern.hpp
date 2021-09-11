#pragma once

#include <string>
#include <vector>

#include <hex/pattern_language/pattern_data.hpp>

#define TEST(name) (hex::test::TestPattern*) new hex::test::TestPattern ## name ()

namespace hex::test {

    using namespace pl;

    enum class Mode {
        Succeeding,
        Failing
    };

    class TestPattern {
    public:
        explicit TestPattern(const std::string &name, Mode mode = Mode::Succeeding) : m_mode(mode) {
            TestPattern::s_tests.insert({ name, this });
        }

        virtual ~TestPattern() {
            for (auto &pattern : this->m_patterns)
                delete pattern;
        }

        template<typename T>
        static T* create(u64 offset, size_t size, const std::string &typeName, const std::string &varName) {
            auto pattern = new T(offset, size);
            pattern->setTypeName(typeName);
            pattern->setVariableName(varName);

            return pattern;
        }

        [[nodiscard]]
        virtual std::string getSourceCode() const = 0;

        [[nodiscard]]
        virtual const std::vector<PatternData*>& getPatterns() const final { return this->m_patterns; }
        virtual void addPattern(PatternData *pattern) final {
            this->m_patterns.push_back(pattern);
        }

        [[nodiscard]]
        auto failing() {
            this->m_mode = Mode::Failing;

            return this;
        }

        [[nodiscard]]
        Mode getMode() {
            return this->m_mode;
        }

        [[nodiscard]]
        static auto& getTests() {
            return TestPattern::s_tests;
        }

    private:
        std::vector<PatternData*> m_patterns;
        Mode m_mode;

        static inline std::map<std::string, TestPattern*> s_tests;
    };

}