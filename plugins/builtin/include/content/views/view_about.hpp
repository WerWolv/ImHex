#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>
#include <hex/plugin.hpp>
#include <hex/helpers/http_requests.hpp>

namespace hex::plugin::builtin {

    class ViewAbout : public View::Modal {
    public:
        ViewAbout();

        void drawContent() override;

        [[nodiscard]] bool shouldDraw() const override { return true; }
        [[nodiscard]] bool hasViewMenuItemEntry() const override { return false; }

        [[nodiscard]] ImGuiWindowFlags getWindowFlags() const override {
            return ImGuiWindowFlags_AlwaysAutoResize;
        }

        ImVec2 getMinSize() const override {
            return scaled({ 700, 450 });
        }

        ImVec2 getMaxSize() const override {
            return scaled({ 700, 450 });
        }

    private:
        void drawAboutPopup();

        void drawAboutMainPage();
        void drawBuildInformation();
        void drawContributorPage();
        void drawLibraryCreditsPage();
        void drawLoadedPlugins();
        void drawPluginRow(const hex::Plugin& plugin);
        void drawPathsPage();
        void drawReleaseNotesPage();
        void drawCommitHistoryPage();
        void drawCommitsTable(const auto& commits);
        void drawCommitRow(const auto& commit);
        void drawLicensePage();

        ImGuiExt::Texture m_logoTexture;

        std::future<HttpRequest::Result<std::string>> m_releaseNoteRequest, m_commitHistoryRequest;
        u32 m_clickCount = 0;
    };

}