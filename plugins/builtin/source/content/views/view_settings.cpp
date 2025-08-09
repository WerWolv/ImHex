#include "content/views/view_settings.hpp"

#include <fonts/fonts.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/helpers/logger.hpp>

#include <nlohmann/json.hpp>

#include <popups/popup_question.hpp>
#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    ViewSettings::ViewSettings() : View::Modal("hex.builtin.view.settings.name", ICON_VS_SETTINGS_GEAR) {
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
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.view.settings.name" }, ICON_VS_SETTINGS_GEAR, 4000, CTRLCMD + Keys::Comma, [&, this] {
            this->getWindowOpenState() = true;
        });

        EventImHexStartupFinished::subscribe(this, []{
            for (const auto &[unlocalizedCategory, unlocalizedDescription, subCategories] : ContentRegistry::Settings::impl::getSettings()) {
                for (const auto &[unlocalizedSubCategory, entries] : subCategories) {
                    for (const auto &[unlocalizedName, widget] : entries) {
                        auto defaultValue = widget->store();
                        try {
                            widget->load(ContentRegistry::Settings::impl::getSetting(unlocalizedCategory, unlocalizedName, defaultValue));
                        } catch (const std::exception &e) {
                            log::error("Failed to load setting [{} / {}]: {}", unlocalizedCategory.get(), unlocalizedName.get(), e.what());

                            ContentRegistry::Settings::impl::getSetting(unlocalizedCategory, unlocalizedName, defaultValue) = defaultValue;
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
                    u32 index = 0;
                    for (auto &subCategory : category.subCategories) {
                        ON_SCOPE_EXIT { index += 1; };

                        // Skip empty subcategories
                        if (subCategory.entries.empty())
                            continue;

                        if (ImGuiExt::BeginSubWindow(Lang(subCategory.unlocalizedName))) {
                            for (auto &setting : subCategory.entries) {
                                ImGui::BeginDisabled(!setting.widget->isEnabled());
                                auto title = Lang(setting.unlocalizedName);
                                ImGui::PushItemWidth(std::min(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(title.get()).x - 20_scaled, 500_scaled));
                                bool settingChanged = setting.widget->draw(title);
                                ImGui::PopItemWidth();
                                ImGui::EndDisabled();

                                if (const auto &tooltip = setting.widget->getTooltip(); tooltip.has_value()) {
                                    ImGui::BeginDisabled();
                                    ImGui::Indent();
                                    fonts::Default().push(0.8F);
                                    ImGuiExt::TextFormattedWrapped(Lang(tooltip.value()));
                                    ImGui::NewLine();
                                    fonts::Default().pop();
                                    ImGui::Unindent();
                                    ImGui::EndDisabled();
                                }

                                auto &widget = setting.widget;

                                // Handle a setting being changed
                                if (settingChanged) {
                                    auto newValue = widget->store();

                                    // Write new value to settings
                                    ContentRegistry::Settings::write<nlohmann::json>(category.unlocalizedName, setting.unlocalizedName, newValue);

                                    // Print a debug message
                                    log::debug("Setting [{} / {}]: Value was changed to {}", category.unlocalizedName.get(), setting.unlocalizedName.get(), nlohmann::to_string(newValue));

                                    // Request a restart if the setting requires it
                                    if (widget->doesRequireRestart()) {
                                        m_restartRequested = true;
                                        m_triggerPopup = true;
                                    }

                                    ContentRegistry::Settings::impl::store();
                                }
                            }

                        }
                        ImGuiExt::EndSubWindow();

                        if (index != i64(category.subCategories.size()) - 1)
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
