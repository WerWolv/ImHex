#pragma once

#include <string>
#include <vector>

#include <hex/pattern_language/pattern_data.hpp>

namespace hex::test {

    class TestPattern {
    public:
        TestPattern() = default;
        virtual ~TestPattern() {
            for (auto &pattern : this->m_patterns)
                delete pattern;
        }

        template<typename T>
        static T* createVariablePattern(u64 offset, size_t size, const std::string &typeName, const std::string &varName) {
            auto pattern = new T(offset, size);
            pattern->setTypeName(typeName);
            pattern->setVariableName(varName);

            return pattern;
        }

        virtual std::string getSourceCode() const = 0;

        [[nodiscard]]
        virtual const std::vector<pl::PatternData*>& getPatterns() const final { return this->m_patterns; }
        virtual void addPattern(pl::PatternData *pattern) final {
            this->m_patterns.push_back(pattern);
        }

    private:
        std::vector<pl::PatternData*> m_patterns;
    };

}