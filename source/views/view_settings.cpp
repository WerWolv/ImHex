#include "views/view_settings.hpp"

#include <hex/api/content_registry.hpp>

namespace hex {

    ViewSettings::ViewSettings() : View("Settings") {
        this->getWindowOpenState() = true;
    }

    ViewSettings::~ViewSettings() {

    }

    void ViewSettings::drawContent() {

        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, FLT_MAX));

        if (ImGui::BeginPopupModal("Preferences", &this->m_settingsWindowOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            for (auto &[category, entries] : ContentRegistry::Settings::getEntries()) {
                ImGui::TextUnformatted(category.c_str());
                ImGui::Separator();
                for (auto &[name, callback] : entries) {
                    ImGui::TextUnformatted(name.c_str());
                    ImGui::SameLine();
                    if (callback(ContentRegistry::Settings::getSettingsData()[category][name]))
                        View::postEvent(Events::SettingsChanged);
                    ImGui::NewLine();
                }
                ImGui::NewLine();
            }
            ImGui::EndPopup();
        }

    }

    void ViewSettings::drawMenu() {
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Preferences")) {
                View::doLater([]{ ImGui::OpenPopup("Preferences"); });
                this->m_settingsWindowOpen = true;
            }
            ImGui::EndMenu();
        }
    }

}