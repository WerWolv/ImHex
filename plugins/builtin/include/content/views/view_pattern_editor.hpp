#pragma once

#include <hex/views/view.hpp>
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

        void drawMenu() override;
        void drawAlwaysVisible() override;
        void drawContent() override;

    private:
        pl::PatternLanguage *m_patternLanguageRuntime;
        std::vector<std::filesystem::path> m_possiblePatternFiles;
        u32 m_selectedPatternFile = 0;
        bool m_runAutomatically = false;
        bool m_evaluatorRunning = false;
        bool m_hasUnevaluatedChanges = false;

        bool m_acceptPatternWindowOpen = false;

        TextEditor m_textEditor;
        std::vector<std::pair<pl::LogConsole::Level, std::string>> m_console;

        void loadPatternFile(const std::string &path);
        void clearPatternData();
        void parsePattern(char *buffer);
    };

}