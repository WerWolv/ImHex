#pragma once

#include <hex/ui/view.hpp>
#include <pl/pattern_language.hpp>
#include <pl/core/errors/error.hpp>
#include <hex/providers/provider.hpp>

#include <cstring>
#include <filesystem>
#include <string_view>
#include <thread>
#include <vector>

#include <TextEditor.h>

namespace pl::ptrn { class Pattern; }

namespace hex::plugin::builtin {

    class ViewPatternEditor : public View {
    public:
        ViewPatternEditor();
        ~ViewPatternEditor() override;

        void drawAlwaysVisible() override;
        void drawContent() override;

    private:
        struct PatternVariable {
            bool inVariable;
            bool outVariable;

            pl::core::Token::ValueType type;
            pl::core::Token::Literal value;
        };

        enum class EnvVarType
        {
            Integer,
            Float,
            String,
            Bool
        };

        struct EnvVar {
            u64 id;
            std::string name;
            pl::core::Token::Literal value;
            EnvVarType type;

            bool operator==(const EnvVar &other) const {
                return this->id == other.id;
            }
        };

        enum class DangerousFunctionPerms : u8 {
            Ask,
            Allow,
            Deny
        };

    private:
        std::unique_ptr<pl::PatternLanguage> m_parserRuntime;

        std::vector<std::fs::path> m_possiblePatternFiles;
        u32 m_selectedPatternFile = 0;
        bool m_runAutomatically   = false;

        bool m_lastEvaluationProcessed = true;
        bool m_lastEvaluationResult    = false;
        std::optional<pl::core::err::PatternLanguageError> m_lastEvaluationError;
        std::vector<std::pair<pl::core::LogConsole::Level, std::string>> m_lastEvaluationLog;
        std::map<std::string, pl::core::Token::Literal> m_lastEvaluationOutVars;

        std::atomic<u32> m_runningEvaluators = 0;
        std::atomic<u32> m_runningParsers    = 0;

        bool m_hasUnevaluatedChanges = false;

        bool m_acceptPatternWindowOpen = false;

        TextEditor m_textEditor;
        std::vector<std::pair<pl::core::LogConsole::Level, std::string>> m_console;

        std::map<std::string, PatternVariable> m_patternVariables;

        u64 m_envVarIdCounter;
        std::list<EnvVar> m_envVarEntries;

        std::atomic<bool> m_dangerousFunctionCalled = false;
        std::atomic<DangerousFunctionPerms> m_dangerousFunctionsAllowed = DangerousFunctionPerms::Ask;

        bool m_syncPatternSourceCode = false;
        bool m_autoLoadPatterns = true;

    private:
        void drawConsole(ImVec2 size);
        void drawEnvVars(ImVec2 size);
        void drawVariableSettings(ImVec2 size);

        void drawPatternTooltip(pl::ptrn::Pattern *pattern);

        void loadPatternFile(const std::fs::path &path);

        void parsePattern(const std::string &code);
        void evaluatePattern(const std::string &code);

        void registerEvents();
        void registerMenuItems();
        void registerHandlers();
    };

}
