#pragma once

#include <string>
#include <vector>

#include <hex/pattern_language/patterns/pattern.hpp>

#define TEST(name) (hex::test::TestPattern *)new hex::test::TestPattern##name()

namespace hex::test {

    using namespace pl;

    enum class Mode
    {
        Succeeding,
        Failing
    };

    class TestPattern {
    public:
        explicit TestPattern(const std::string &name, Mode mode = Mode::Succeeding) : m_mode(mode) {
            TestPattern::s_tests.insert({ name, this });
        }

        virtual ~TestPattern() = default;

        template<typename T>
        static std::unique_ptr<T> create(const std::string &typeName, const std::string &varName, auto... args) {
            auto pattern = std::make_unique<T>(nullptr, args...);
            pattern->setTypeName(typeName);
            pattern->setVariableName(varName);

            return std::move(pattern);
        }

        [[nodiscard]] virtual std::string getSourceCode() const = 0;

        [[nodiscard]] virtual const std::vector<std::unique_ptr<Pattern>> &getPatterns() const final { return this->m_patterns; }
        virtual void addPattern(std::unique_ptr<Pattern> &&pattern) final {
            this->m_patterns.push_back(std::move(pattern));
        }

        [[nodiscard]] auto failing() {
            this->m_mode = Mode::Failing;

            return this;
        }

        [[nodiscard]] Mode getMode() {
            return this->m_mode;
        }

        [[nodiscard]] static auto &getTests() {
            return TestPattern::s_tests;
        }

    private:
        std::vector<std::unique_ptr<Pattern>> m_patterns;
        Mode m_mode;

        static inline std::map<std::string, TestPattern *> s_tests;
    };

}