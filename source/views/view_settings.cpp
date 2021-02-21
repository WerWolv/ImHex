#include "views/view_settings.hpp"

#include <hex/api/content_registry.hpp>

namespace hex {

    ViewSettings::ViewSettings() : View("hex.view.settings.name"_lang) {
        View::subscribeEvent(Events::OpenWindow, [this](auto name) {
            if (std::any_cast<const char*>(name) == std::string("hex.view.settings.name")) {
                View::doLater([]{ ImGui::OpenPopup("hex.view.settings.name"_lang); });
                this->getWindowOpenState() = true;
            }
        });
    }

    ViewSettings::~ViewSettings() {
        View::unsubscribeEvent(Events::OpenWindow);
    }

    void ViewSettings::drawContent() {

        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, FLT_MAX));

        if (ImGui::BeginPopupModal("hex.view.settings.name"_lang, &this->getWindowOpenState(), ImGuiWindowFlags_AlwaysAutoResize)) {
            for (auto &[category, entries] : ContentRegistry::Settings::getEntries()) {
                ImGui::TextUnformatted(LangEntry(category));
                ImGui::Separator();
                for (auto &[name, callback] : entries) {
                    if (callback(LangEntry(name), ContentRegistry::Settings::getSettingsData()[category][name]))
                        View::postEvent(Events::SettingsChanged);
                }
                ImGui::NewLine();
            }
            ImGui::EndPopup();
        } else
            this->getWindowOpenState() = false;

    }

    void ViewSettings::drawMenu() {
        if (ImGui::BeginMenu("hex.menu.help"_lang)) {
            if (ImGui::MenuItem("hex.view.settings.name"_lang)) {
                View::doLater([]{ ImGui::OpenPopup("hex.view.settings.name"_lang); });
                this->getWindowOpenState() = true;
            }
            ImGui::EndMenu();
        }
    }

}