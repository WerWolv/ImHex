#include "content/views/view_settings.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <nlohmann/json.hpp>

#include <content/popups/popup_question.hpp>

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

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.extras" }, 3000);

        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.view.settings.name"_lang }, 4000, Shortcut::None, [&, this] {
            TaskManager::doLater([] { ImGui::OpenPopup(View::toWindowName("hex.builtin.view.settings.name").c_str()); });
            this->getWindowOpenState() = true;
        });
    }

    ViewSettings::~ViewSettings() {
        EventManager::unsubscribe<RequestOpenWindow>(this);
    }

    void ViewSettings::drawContent() {

        if (ImGui::BeginPopupModal(View::toWindowName("hex.builtin.view.settings.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoResize)) {
            if (ImGui::BeginTabBar("settings")) {
                auto &entries = ContentRegistry::Settings::impl::getEntries();

                std::vector<std::decay_t<decltype(entries)>::const_iterator> sortedCategories;

                for (auto it = entries.cbegin(); it != entries.cend(); it++) {
                    sortedCategories.emplace_back(it);
                }

                std::sort(sortedCategories.begin(), sortedCategories.end(), [](auto &item0, auto &item1) {
                    return item0->first.slot < item1->first.slot;
                });

                const auto &descriptions = ContentRegistry::Settings::impl::getCategoryDescriptions();

                for (auto &it : sortedCategories) {
                    auto &[category, settings] = *it;
                    if (ImGui::BeginTabItem(LangEntry(category.name))) {
                        const std::string &categoryDesc = descriptions.contains(category.name) ? descriptions.at(category.name) : category.name;

                        LangEntry descriptionEntry{categoryDesc};
                        ImGui::TextFormattedWrapped("{}", descriptionEntry);
                        ImGui::InfoTooltip(descriptionEntry);
                        ImGui::Separator();

                        for (auto &[name, requiresRestart, callback] : settings) {
                            auto &setting = ContentRegistry::Settings::impl::getSettingsData()[category.name][name];
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

        if (!this->getWindowOpenState() && this->m_restartRequested) {
            PopupQuestion::open("hex.builtin.view.settings.restart_question"_lang, ImHexApi::System::restartImHex, []{});
        }
    }

}
