#include "content/views/view_settings.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <nlohmann/json.hpp>

#include <content/popups/popup_question.hpp>

namespace hex::plugin::builtin {

    ViewSettings::ViewSettings() : View("hex.builtin.view.settings.name") {
        // Handle window open requests
        EventManager::subscribe<RequestOpenWindow>(this, [this](const std::string &name) {
            if (name == "Settings") {
                TaskManager::doLater([this] {
                    ImGui::OpenPopup(View::toWindowName("hex.builtin.view.settings.name").c_str());
                    this->getWindowOpenState() = true;
                });
            }
        });

        // Add the settings menu item to the Extras menu
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
                auto &categories = ContentRegistry::Settings::impl::getSettings();

                // Draw all categories
                for (auto &category : categories) {

                    // Skip empty categories
                    if (category.subCategories.empty())
                        continue;

                    // For each category, create a new tab
                    if (ImGui::BeginTabItem(LangEntry(category.unlocalizedName))) {
                        // Draw the category description
                        if (!category.unlocalizedDescription.empty()) {
                            ImGuiExt::TextFormattedWrapped("{}", LangEntry(category.unlocalizedDescription));
                            ImGui::NewLine();
                        }

                        bool firstSubCategory = true;

                        // Draw all settings of that category
                        for (auto &subCategory : category.subCategories) {

                            // Skip empty subcategories
                            if (subCategory.entries.empty())
                                continue;

                            if (!subCategory.unlocalizedName.empty())
                                ImGuiExt::Header(LangEntry(subCategory.unlocalizedName), firstSubCategory);

                            firstSubCategory = false;

                            if (ImGuiExt::BeginBox()) {
                                for (auto &setting : subCategory.entries) {
                                    ImGui::BeginDisabled(!setting.widget->isEnabled());
                                    bool settingChanged = setting.widget->draw(LangEntry(setting.unlocalizedName));
                                    ImGui::EndDisabled();

                                    if (auto tooltip = setting.widget->getTooltip(); tooltip.has_value() && ImGui::IsItemHovered())
                                        ImGuiExt::InfoTooltip(LangEntry(tooltip.value()));

                                    auto &widget = setting.widget;

                                    // Handle a setting being changed
                                    if (settingChanged) {
                                        auto newValue = widget->store();

                                        // Write new value to settings
                                        ContentRegistry::Settings::write(category.unlocalizedName, setting.unlocalizedName, newValue);

                                        // Print a debug message
                                        log::debug("Setting [{} / {}]: Value was changed to {}", category.unlocalizedName, setting.unlocalizedName, nlohmann::to_string(newValue));

                                        // Signal that the setting was changed
                                        EventManager::post<EventSettingsChanged>();
                                        widget->onChanged();

                                        // Request a restart if the setting requires it
                                        if (widget->doesRequireRestart())
                                            this->m_restartRequested = true;
                                    }
                                }

                                ImGuiExt::EndBox();
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

        // If a restart is required, ask the user if they want to restart
        if (!this->getWindowOpenState() && this->m_restartRequested) {
            PopupQuestion::open("hex.builtin.view.settings.restart_question"_lang, ImHexApi::System::restartImHex, []{});
        }
    }

}
