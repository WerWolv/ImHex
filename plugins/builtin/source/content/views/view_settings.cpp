#include "content/views/view_settings.hpp"

#include <hex/api/content_registry.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    ViewSettings::ViewSettings() : View("hex.builtin.view.settings.name") {
        EventManager::subscribe<RequestOpenWindow>(this, [this](const std::string &name) {
            if (name == "Settings") {
                View::doLater([] { ImGui::OpenPopup(View::toWindowName("hex.builtin.view.settings.name").c_str()); });
                this->getWindowOpenState() = true;
            }
        });

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.help", 2000, [&, this] {
            if (ImGui::MenuItem("hex.builtin.view.settings.name"_lang)) {
                View::doLater([] { ImGui::OpenPopup(View::toWindowName("hex.builtin.view.settings.name").c_str()); });
                this->getWindowOpenState() = true;
            }
        });
    }

    ViewSettings::~ViewSettings() {
        EventManager::unsubscribe<RequestOpenWindow>(this);
    }

    void ViewSettings::drawContent() {

        if (ImGui::BeginPopupModal(View::toWindowName("hex.builtin.view.settings.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoResize)) {
            if (ImGui::BeginTabBar("settings")) {
                for (auto &[category, entries] : ContentRegistry::Settings::getEntries()) {
                    if (ImGui::BeginTabItem(LangEntry(category))) {
                        ImGui::TextUnformatted(LangEntry(category));
                        ImGui::Separator();

                        for (auto &[name, callback] : entries) {
                            if (callback(LangEntry(name), ContentRegistry::Settings::getSettingsData()[category][name]))
                                EventManager::post<EventSettingsChanged>();
                        }

                        ImGui::EndTabItem();
                    }
                }

                ImGui::EndTabBar();
            }
            ImGui::EndPopup();
        } else
            this->getWindowOpenState() = false;
    }

}