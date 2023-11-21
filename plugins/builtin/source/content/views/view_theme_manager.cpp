#include "content/views/view_theme_manager.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/theme_manager.hpp>

#include <wolv/io/file.hpp>

namespace hex::plugin::builtin {

    ViewThemeManager::ViewThemeManager() : View::Floating("hex.builtin.view.theme_manager.name") {
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.extras", "hex.builtin.view.theme_manager.name" }, 2000, Shortcut::None, [&, this] {
            this->getWindowOpenState() = true;
        });
    }

    void ViewThemeManager::drawContent() {
        ImGuiExt::Header("hex.builtin.view.theme_manager.colors"_lang, true);

        // Draw theme handlers
        ImGui::PushID(1);


        // Loop over each theme handler
        bool anyColorHovered = false;
        for (auto &[name, handler] : ThemeManager::getThemeHandlers()) {
            // Create a new collapsable header for each category
            if (ImGui::CollapsingHeader(name.c_str())) {

                // Loop over all the individual theme settings
                for (auto &[colorName, colorId] : handler.colorMap) {
                    if (this->m_startingColor.has_value()) {
                        if (this->m_hoveredColorId == colorId && this->m_hoveredHandlerName == name) {
                            handler.setFunction(colorId, this->m_startingColor.value());
                        }
                    }

                    // Get the current color value
                    auto color = handler.getFunction(colorId);

                    // Draw a color picker for the color
                    if (ImGui::ColorEdit4(colorName.c_str(), reinterpret_cast<float*>(&color.Value), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
                        // Update the color value
                        handler.setFunction(colorId, color);
                        EventManager::post<EventThemeChanged>();
                    }

                    if (ImGui::IsItemHovered()) {
                        anyColorHovered = true;

                        if (!this->m_hoveredColorId.has_value()) {
                            this->m_hoveredColorId      = colorId;
                            this->m_startingColor       = color;
                            this->m_hoveredHandlerName  = name;
                        }
                    }
                }
            }

            if (this->m_hoveredHandlerName == name && this->m_startingColor.has_value() && this->m_hoveredColorId.has_value()) {
                auto flashingColor = this->m_startingColor.value();

                const float flashProgress = std::min(1.0F, (1.0F + sinf(ImGui::GetTime() * 6)) / 2.0F);
                flashingColor.Value.x = std::lerp(flashingColor.Value.x / 2, 1.0F, flashProgress);
                flashingColor.Value.y = std::lerp(flashingColor.Value.y / 2, 1.0F, flashProgress);
                flashingColor.Value.z = flashingColor.Value.z / 2;
                flashingColor.Value.w = 1.0F;

                handler.setFunction(*this->m_hoveredColorId, flashingColor);

                if (!anyColorHovered) {
                    handler.setFunction(this->m_hoveredColorId.value(), this->m_startingColor.value());
                    this->m_startingColor.reset();
                    this->m_hoveredColorId.reset();
                }
            }
        }
        ImGui::PopID();


        ImGuiExt::Header("hex.builtin.view.theme_manager.styles"_lang);

        // Draw style handlers
        ImGui::PushID(2);

        // Loop over each style handler
        for (auto &[name, handler] : ThemeManager::getStyleHandlers()) {
            // Create a new collapsable header for each category
            if (ImGui::CollapsingHeader(name.c_str())) {

                // Loop over all the individual style settings
                for (auto &[styleName, style] : handler.styleMap) {
                    // Get the current style value
                    auto &[value, min, max, needsScaling] = style;

                    // Styles can either be floats or ImVec2s
                    // Determine which one it is and draw the appropriate slider
                    if (auto floatValue = std::get_if<float*>(&value); floatValue != nullptr) {
                        if (ImGui::SliderFloat(styleName.c_str(), *floatValue, min, max, "%.1f")) {
                            EventManager::post<EventThemeChanged>();
                        }
                    } else if (auto vecValue = std::get_if<ImVec2*>(&value); vecValue != nullptr) {
                        if (ImGui::SliderFloat2(styleName.c_str(), &(*vecValue)->x, min, max, "%.1f")) {
                            EventManager::post<EventThemeChanged>();
                        }
                    }
                }
            }
        }
        ImGui::PopID();

        // Draw export settings
        ImGuiExt::Header("hex.builtin.view.theme_manager.export"_lang);
        ImGuiExt::InputTextIcon("hex.builtin.view.theme_manager.export.name"_lang, ICON_VS_SYMBOL_KEY, this->m_themeName);

        // Draw the export buttons
        if (ImGui::Button("hex.builtin.view.theme_manager.save_theme"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            fs::openFileBrowser(fs::DialogMode::Save, { { "ImHex Theme", "json" } }, [this](const std::fs::path &path){
                // Export the current theme as json
                auto json = ThemeManager::exportCurrentTheme(this->m_themeName);

                // Write the json to the file
                wolv::io::File outputFile(path, wolv::io::File::Mode::Create);
                outputFile.writeString(json.dump(4));
            });
        }
    }

}