#pragma once

#include <content/providers/file_provider.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/magic.hpp>
#include <hex/ui/view.hpp>
#include <ui/markdown.hpp>

namespace hex::plugin::builtin {

    class ViewFullScreenFileInfo : public View::FullScreen {
    public:
        explicit ViewFullScreenFileInfo(std::fs::path filePath);

        void drawContent() override;

    private:
        std::fs::path m_filePath;
        FileProvider m_provider;
        TaskHolder m_analysisTask;

        std::string m_mimeType;
        std::string m_fileDescription;
        std::vector<magic::FoundPattern> m_foundPatterns;
        std::optional<ui::Markdown> m_fullDescription;
    };

}
