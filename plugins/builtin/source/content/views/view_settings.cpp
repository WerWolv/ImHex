#include "content/views/view_settings.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/helpers/logger.hpp>

#include <nlohmann/json.hpp>

#include <popups/popup_question.hpp>
#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    ViewSettings::ViewSettings() : View::Modal("hex.builtin.view.settings.name") {
        // Handle window open requests
        RequestOpenWindow::subscribe(this, [this](const std::string &name) {
            if (name == "Settings") {
                TaskManager::doLater([this] {
                    this->getWindowOpenState() = true;
                });
            }
        });

        // Add the settings menu item to the Extras menu
        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.extras" }, 3000);
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.view.settings.name" }, ICON_VS_SETTINGS_GEAR, 4000, Shortcut::None, [&, this] {
            this->getWindowOpenState() = true;
        });

        EventImHexStartupFinished::subscribe(this, []{
            for (const auto &[unlocalizedCategory, unlocalizedDescription, subCategories] : ContentRegistry::Settings::impl::getSettings()) {
                for (const auto &[unlocalizedSubCategory, entries] : subCategories) {
                    for (const auto &[unlocalizedName, widget] : entries) {
                        try {
                            auto defaultValue = widget->store();
                            widget->load(ContentRegistry::Settings::impl::getSetting(unlocalizedCategory, unlocalizedName, defaultValue));
                            widget->onChanged();
                        } catch (const std::exception &e) {
                            log::error("Failed to load setting [{} / {}]: {}", unlocalizedCategory.get(), unlocalizedName.get(), e.what());
                        }
                    }
                }
            }
        });
    }

    ViewSettings::~ViewSettings() {
        RequestOpenWindow::unsubscribe(this);
        EventImHexStartupFinished::unsubscribe(this);
    }

    void ViewSettings::drawContent() {
        if (ImGui::BeginTable("Settings", 2, ImGuiTableFlags_BordersInner)) {
            ImGui::TableSetupColumn("##category", ImGuiTableColumnFlags_WidthFixed, 120_scaled);
            ImGui::TableSetupColumn("##settings", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            {
                auto &categories = ContentRegistry::Settings::impl::getSettings();

                // Draw all categories
                bool categorySet = false;
                for (auto &category : categories) {
                    // Skip empty categories
                    if (category.subCategories.empty())
                        continue;

                    if (ImGui::Selectable(Lang(category.unlocalizedName), m_selectedCategory == &category, ImGuiSelectableFlags_NoAutoClosePopups) || m_selectedCategory == nullptr)
                        m_selectedCategory = &category;

                    if (m_selectedCategory == &category)
                        categorySet = true;
                }

                if (!categorySet)
                    m_selectedCategory = nullptr;
            }

            ImGui::TableNextColumn();

            if (m_selectedCategory != nullptr) {
                auto &category = *m_selectedCategory;

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

                        if (ImGuiExt::BeginSubWindow(Lang(subCategory.unlocalizedName))) {
                            for (auto &setting : subCategory.entries) {
                                ImGui::BeginDisabled(!setting.widget->isEnabled());
                                ImGui::PushItemWidth(-200_scaled);
                                bool settingChanged = setting.widget->draw(Lang(setting.unlocalizedName));
                                ImGui::PopItemWidth();
                                ImGui::EndDisabled();

                                if (const auto &tooltip = setting.widget->getTooltip(); tooltip.has_value() && ImGui::IsItemHovered())
                                    ImGuiExt::InfoTooltip(Lang(tooltip.value()));

                                auto &widget = setting.widget;

                                // Handle a setting being changed
                                if (settingChanged) {
                                    auto newValue = widget->store();

                                    // Write new value to settings
                                    ContentRegistry::Settings::write<nlohmann::json>(category.unlocalizedName, setting.unlocalizedName, newValue);

                                    // Print a debug message
                                    log::debug("Setting [{} / {}]: Value was changed to {}", category.unlocalizedName.get(), setting.unlocalizedName.get(), nlohmann::to_string(newValue));

                                    // Signal that the setting was changed
                                    widget->onChanged();

                                    // Request a restart if the setting requires it
                                    if (widget->doesRequireRestart()) {
                                        m_restartRequested = true;
                                        m_triggerPopup = true;
                                    }
                                }
                            }

                        }
                        ImGuiExt::EndSubWindow();
                        ImGui::NewLine();
                    }
                }
                ImGui::EndChild();
            }

            ImGui::EndTable();
        }
    }

    void ViewSettings::drawAlwaysVisibleContent() {
        // If a restart is required, ask the user if they want to restart
        if (!this->getWindowOpenState() && m_triggerPopup) {
            m_triggerPopup = false;
            ui::PopupQuestion::open("hex.builtin.view.settings.restart_question"_lang,
                ImHexApi::System::restartImHex,
                [this]{
                    m_restartRequested = false;
                }
            );
        }
    }


}
