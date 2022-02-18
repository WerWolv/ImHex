#include "content/views/view_settings.hpp"

#include <hex/api/content_registry.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    ViewSettings::ViewSettings() : View("hex.builtin.view.settings.name") {
        EventManager::subscribe<RequestOpenWindow>(this, [this](const std::string &name) {
            if (name == "Settings") {
                ImHexApi::Tasks::doLater([] { ImGui::OpenPopup(View::toWindowName("hex.builtin.view.settings.name").c_str()); });
                this->getWindowOpenState() = true;
            }
        });

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.help", 2000, [&, this] {
            if (ImGui::MenuItem("hex.builtin.view.settings.name"_lang)) {
                ImHexApi::Tasks::doLater([] { ImGui::OpenPopup(View::toWindowName("hex.builtin.view.settings.name").c_str()); });
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
                auto &entries = ContentRegistry::Settings::getEntries();

                std::vector<std::decay_t<decltype(entries)>::const_iterator> sortedCategories;

                for (auto it = entries.cbegin(); it != entries.cend(); it++) {
                    sortedCategories.emplace_back(std::move(it));
                }

                std::sort(sortedCategories.begin(), sortedCategories.end(), [](auto &item0, auto &item1) {
                    return item0->first.slot < item1->first.slot;
                });

                const auto &descriptions = ContentRegistry::Settings::getCategoryDescriptions();

                for (auto &it : sortedCategories) {
                    auto &[category, entries] = *it;
                    if (ImGui::BeginTabItem(LangEntry(category))) {
                        const std::string &categoryDesc = descriptions.count(category) ? descriptions.at(category) : category.name;
                        ImGui::TextUnformatted(LangEntry(categoryDesc));
                        ImGui::Separator();

                        for (auto &[name, callback] : entries) {
                            if (callback(LangEntry(name), ContentRegistry::Settings::getSettingsData()[category.name][name]))
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
