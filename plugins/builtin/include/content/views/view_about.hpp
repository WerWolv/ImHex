#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>
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

    private:
        void drawAboutPopup();

        void drawAboutMainPage();
        void drawContributorPage();
        void drawLibraryCreditsPage();
        void drawPathsPage();
        void drawReleaseNotesPage();
        void drawCommitHistoryPage();
        void drawLicensePage();

        ImGuiExt::Texture m_logoTexture;

        std::future<HttpRequest::Result<std::string>> m_releaseNoteRequest, m_commitHistoryRequest;
        u32 m_clickCount = 0;
    };

}