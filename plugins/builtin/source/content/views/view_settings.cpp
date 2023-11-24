#include "content/views/view_settings.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <nlohmann/json.hpp>

#include <content/popups/popup_question.hpp>
#include <wolv/utils/guards.hpp>

namespace hex::plugin::builtin {

    ViewSettings::ViewSettings() : View::Floating("hex.builtin.view.settings.name") {
        // Handle window open requests
        EventManager::subscribe<RequestOpenWindow>(this, [this](const std::string &name) {
            if (name == "Settings") {
                TaskManager::doLater([this] {
                    ImGui::OpenPopup(View::toWindowName(this->getUnlocalizedName()).c_str());
                    this->getWindowOpenState() = true;
                });
            }
        });

        // Add the settings menu item to the Extras menu
        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.extras" }, 3000);
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.view.settings.name"_lang }, 4000, Shortcut::None, [&, this] {
            TaskManager::doLater([this] { ImGui::OpenPopup(View::toWindowName(this->getUnlocalizedName()).c_str()); });
            this->getWindowOpenState() = true;
        });
    }

    ViewSettings::~ViewSettings() {
        EventManager::unsubscribe<RequestOpenWindow>(this);
    }

    void ViewSettings::drawAlwaysVisibleContent() {
        if (ImGui::BeginPopupModal(View::toWindowName(this->getUnlocalizedName()).c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoResize)) {
            if (ImGui::BeginTabBar("settings")) {
                auto &categories = ContentRegistry::Settings::impl::getSettings();

                // Draw all categories
                for (auto &category : categories) {

                    // Skip empty categories
                    if (category.subCategories.empty())
                        continue;

                    // For each category, create a new tab
                    if (ImGui::BeginTabItem(Lang(category.unlocalizedName))) {
                        if (ImGui::BeginChild("scrolling")) {

                            // Draw the category description
                            if (!category.unlocalizedDescription.empty()) {
                                ImGuiExt::TextFormattedWrapped("{}", Lang(category.unlocalizedDescription));
                                ImGui::NewLine();
                            }

                            // Draw all settings of that category
                            for (auto &subCategory : category.subCategories) {

                                // Skip empty subcategories
                                if (subCategory.entries.empty())
                                    continue;

                                ImGuiExt::BeginSubWindow(Lang(subCategory.unlocalizedName));
                                {
                                    for (auto &setting : subCategory.entries) {
                                        ImGui::BeginDisabled(!setting.widget->isEnabled());
                                        ImGui::PushItemWidth(-200_scaled);
                                        bool settingChanged = setting.widget->draw(Lang(setting.unlocalizedName));
                                        ImGui::PopItemWidth();
                                        ImGui::EndDisabled();

                                        if (auto tooltip = setting.widget->getTooltip(); tooltip.has_value() && ImGui::IsItemHovered())
                                            ImGuiExt::InfoTooltip(Lang(tooltip.value()));

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
                                }
                                ImGuiExt::EndSubWindow();
                            }
                        }
                        ImGui::EndChild();

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
            PopupQuestion::open("hex.builtin.view.settings.restart_question"_lang,
                ImHexApi::System::restartImHex,
                [this]{
                    this->m_restartRequested = false;
                }
            );
        }
    }

}
