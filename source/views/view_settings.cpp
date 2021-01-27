#include "views/view_settings.hpp"

#include <hex/api/content_registry.hpp>

namespace hex {

    ViewSettings::ViewSettings() : View("Settings") {
        View::subscribeEvent(Events::OpenWindow, [this](auto name) {
            if (std::any_cast<const char*>(name) == std::string("Preferences")) {
                View::doLater([]{ ImGui::OpenPopup("Preferences"); });
                this->getWindowOpenState() = true;
            }
        });
    }

    ViewSettings::~ViewSettings() {
        View::unsubscribeEvent(Events::OpenWindow);
    }

    void ViewSettings::drawContent() {

        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, FLT_MAX));

        if (ImGui::BeginPopupModal("Preferences", &this->getWindowOpenState(), ImGuiWindowFlags_AlwaysAutoResize)) {
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
        } else
            this->getWindowOpenState() = false;

    }

    void ViewSettings::drawMenu() {
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Preferences")) {
                View::doLater([]{ ImGui::OpenPopup("Preferences"); });
                this->getWindowOpenState() = true;
            }
            ImGui::EndMenu();
        }
    }

}