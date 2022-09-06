#include "content/views/view_settings.hpp"

#include <hex/api/content_registry.hpp>

#include <hex/helpers/logger.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    ViewSettings::ViewSettings() : View("hex.builtin.view.settings.name") {
        EventManager::subscribe<RequestOpenWindow>(this, [this](const std::string &name) {
            if (name == "Settings") {
                TaskManager::doLater([this] {
                    ImGui::OpenPopup(View::toWindowName("hex.builtin.view.settings.name").c_str());
                    this->getWindowOpenState() = true;
                });
            }
        });

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.help", 2000, [&, this] {
            if (ImGui::MenuItem("hex.builtin.view.settings.name"_lang)) {
                TaskManager::doLater([] { ImGui::OpenPopup(View::toWindowName("hex.builtin.view.settings.name").c_str()); });
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
                    sortedCategories.emplace_back(it);
                }

                std::sort(sortedCategories.begin(), sortedCategories.end(), [](auto &item0, auto &item1) {
                    return item0->first.slot < item1->first.slot;
                });

                const auto &descriptions = ContentRegistry::Settings::getCategoryDescriptions();

                for (auto &it : sortedCategories) {
                    auto &[category, settings] = *it;
                    if (ImGui::BeginTabItem(LangEntry(category))) {
                        const std::string &categoryDesc = descriptions.count(category) ? descriptions.at(category) : category.name;
                        LangEntry descriptionEntry{categoryDesc};
                        ImGui::TextWrapped("%s", static_cast<const char*>(descriptionEntry));
                        ImGui::InfoTooltip(descriptionEntry);
                        ImGui::Separator();

                        for (auto &[name, requiresRestart, callback] : settings) {
                            auto &setting = ContentRegistry::Settings::getSettingsData()[category.name][name];
                            if (callback(LangEntry(name), setting)) {
                                log::debug("Setting [{}]: {} was changed to {}", category.name, name, [&] -> std::string{
                                   if (setting.is_number())
                                       return std::to_string(setting.get<int>());
                                   else if (setting.is_string())
                                       return setting.get<std::string>();
                                   else
                                       return "";
                                }());
                                EventManager::post<EventSettingsChanged>();

                                if (requiresRestart)
                                    this->m_restartRequested = true;
                            }
                        }

                        ImGui::EndTabItem();
                    }
                }

                ImGui::EndTabBar();
            }
            ImGui::EndPopup();
        } else
            this->getWindowOpenState() = false;

        if (this->getWindowOpenState() == false && this->m_restartRequested) {
            View::showYesNoQuestionPopup("hex.builtin.view.settings.restart_question"_lang, ImHexApi::Common::restartImHex, [] {});
        }
    }

}
