#pragma once

#include <hex/ui/view.hpp>
#include <hex/pattern_language/pattern_language.hpp>
#include <hex/pattern_language/log_console.hpp>
#include <hex/providers/provider.hpp>

#include <cstring>
#include <filesystem>
#include <string_view>
#include <thread>
#include <vector>

#include <TextEditor.h>

namespace hex::plugin::builtin {

    class ViewPatternEditor : public View {
    public:
        ViewPatternEditor();
        ~ViewPatternEditor() override;

        void drawAlwaysVisible() override;
        void drawContent() override;

    private:
        pl::PatternLanguage *m_parserRuntime;

        std::vector<fs::path> m_possiblePatternFiles;
        u32 m_selectedPatternFile = 0;
        bool m_runAutomatically = false;

        std::atomic<u32> m_runningEvaluators = 0;
        std::atomic<u32> m_runningParsers = 0;

        bool m_hasUnevaluatedChanges = false;

        bool m_acceptPatternWindowOpen = false;

        TextEditor m_textEditor;
        std::vector<std::pair<pl::LogConsole::Level, std::string>> m_console;

        struct PatternVariable {
            bool inVariable;
            bool outVariable;

            pl::Token::ValueType type;
            pl::Token::Literal value;
        };

        std::map<std::string, PatternVariable> m_patternVariables;
        std::vector<std::string> m_patternTypes;

        enum class EnvVarType {
            Integer,
            Float,
            String,
            Bool
        };

        struct EnvVar {
            u64 id;
            std::string name;
            pl::Token::Literal value;
            EnvVarType type;

            bool operator==(const EnvVar &other) const {
                return this->id == other.id;
            }
        };

        u64 m_envVarIdCounter;
        std::list<EnvVar> m_envVarEntries;

        void drawConsole(ImVec2 size);
        void drawEnvVars(ImVec2 size);
        void drawVariableSettings(ImVec2 size);

        void loadPatternFile(const fs::path &path);
        void clearPatternData();

        void parsePattern(const std::string &code);
        void evaluatePattern(const std::string &code);
    };

}