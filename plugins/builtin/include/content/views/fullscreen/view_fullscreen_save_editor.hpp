#pragma once

#include <hex/ui/view.hpp>
#include <content/providers/file_provider.hpp>
#include <pl/pattern_language.hpp>
#include <ui/pattern_value_editor.hpp>

namespace hex::plugin::builtin {

    class ViewFullScreenSaveEditor : public View::FullScreen {
    public:
        explicit ViewFullScreenSaveEditor(std::string sourceCode);

        void drawContent() override;

    private:
        void drawFileSelectScreen();
        void drawSaveEditorScreen();

    private:
        std::string m_sourceCode;
        FileProvider m_provider;

        pl::PatternLanguage m_runtime;
        ui::PatternValueEditor m_saveEditor;

        std::string m_saveEditorName;
        std::vector<std::string> m_saveEditorAuthors;
        std::vector<std::string> m_saveEditorDescriptions;
    };

}
