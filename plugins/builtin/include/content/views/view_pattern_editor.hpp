#pragma once

#include <hex/ui/view.hpp>
#include <pl/pattern_language.hpp>
#include <pl/core/errors/error.hpp>
#include <hex/providers/provider.hpp>

#include <content/helpers/provider_extra_data.hpp>
#include <content/helpers/hex_editor.hpp>
#include <content/providers/memory_file_provider.hpp>

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
        enum class DangerousFunctionPerms : u8 {
            Ask,
            Allow,
            Deny
        };

    private:
        using PlData = ProviderExtraData::Data::PatternLanguage;

        std::unique_ptr<pl::PatternLanguage> m_parserRuntime;

        std::vector<std::fs::path> m_possiblePatternFiles;
        u32 m_selectedPatternFile = 0;
        bool m_runAutomatically   = false;

        bool m_lastEvaluationProcessed = true;
        bool m_lastEvaluationResult    = false;

        std::atomic<u32> m_runningEvaluators = 0;
        std::atomic<u32> m_runningParsers    = 0;

        bool m_hasUnevaluatedChanges = false;

        bool m_acceptPatternWindowOpen = false;

        TextEditor m_textEditor;

        std::atomic<bool> m_dangerousFunctionCalled = false;
        std::atomic<DangerousFunctionPerms> m_dangerousFunctionsAllowed = DangerousFunctionPerms::Ask;

        bool m_syncPatternSourceCode = false;
        bool m_autoLoadPatterns = true;

        std::unique_ptr<MemoryFileProvider> m_sectionProvider = nullptr;
        HexEditor m_hexEditor;

    private:
        void drawConsole(ImVec2 size, const std::vector<std::pair<pl::core::LogConsole::Level, std::string>> &console);
        void drawEnvVars(ImVec2 size, std::list<PlData::EnvVar> &envVars);
        void drawVariableSettings(ImVec2 size, std::map<std::string, PlData::PatternVariable> &patternVariables);
        void drawSectionSelector(ImVec2 size, std::map<u64, pl::api::Section> &sections);

        void drawPatternTooltip(pl::ptrn::Pattern *pattern);

        void loadPatternFile(const std::fs::path &path, prv::Provider *provider);

        void parsePattern(const std::string &code, prv::Provider *provider);
        void evaluatePattern(const std::string &code, prv::Provider *provider);

        void registerEvents();
        void registerMenuItems();
        void registerHandlers();
    };

}
