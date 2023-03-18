#include "content/views/view_theme_manager.hpp"

#include <hex/api/theme_manager.hpp>

#include <wolv/io/file.hpp>

namespace hex::plugin::builtin {

    ViewThemeManager::ViewThemeManager() : View("hex.builtin.view.theme_manager.name") {
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.help", 1200, [&, this] {
            if (ImGui::MenuItem("hex.builtin.view.theme_manager.name"_lang, "")) {
                this->m_viewOpen = true;
                this->getWindowOpenState() = true;
            }
        });
    }

    void ViewThemeManager::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.theme_manager.name").c_str(), &this->m_viewOpen, ImGuiWindowFlags_NoCollapse)) {
            ImGui::Header("hex.builtin.view.theme_manager.colors"_lang, true);

            ImGui::PushID(1);
            const auto &themeHandlers = api::ThemeManager::getThemeHandlers();
            for (auto &[name, handler] : themeHandlers) {
                if (ImGui::CollapsingHeader(name.c_str())) {
                    for (auto &[colorName, colorId] : handler.colorMap) {
                        auto color = handler.getFunction(colorId);
                        if (ImGui::ColorEdit4(colorName.c_str(), (float*)&color.Value,
                                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf))
                        {
                            handler.setFunction(colorId, color);
                        }
                    }
                }
            }
            ImGui::PopID();


            ImGui::Header("hex.builtin.view.theme_manager.styles"_lang);

            ImGui::PushID(2);
            for (auto &[name, handler] : api::ThemeManager::getStyleHandlers()) {
                if (ImGui::CollapsingHeader(name.c_str())) {
                    for (auto &[styleName, style] : handler.styleMap) {
                        auto &[value, min, max, needsScaling] = style;

                        if (auto floatValue = std::get_if<float*>(&value); floatValue != nullptr)
                            ImGui::SliderFloat(styleName.c_str(), *floatValue, min, max, "%.1f");
                        else if (auto vecValue = std::get_if<ImVec2*>(&value); vecValue != nullptr)
                            ImGui::SliderFloat2(styleName.c_str(), &(*vecValue)->x, min, max, "%.1f");
                    }
                }
            }
            ImGui::PopID();

            ImGui::Header("hex.builtin.view.theme_manager.export"_lang);
            ImGui::InputTextIcon("hex.builtin.view.theme_manager.export.name"_lang, ICON_VS_SYMBOL_KEY, this->m_themeName);
            if (ImGui::Button("hex.builtin.view.theme_manager.save_theme"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                fs::openFileBrowser(fs::DialogMode::Save, { { "ImHex Theme", "json" } }, [this](const std::fs::path &path){
                    auto json = api::ThemeManager::exportCurrentTheme(this->m_themeName);

                    wolv::io::File outputFile(path, wolv::io::File::Mode::Create);
                    outputFile.write(json.dump(4));
                });
            }

        }
        ImGui::End();

        this->getWindowOpenState() = this->m_viewOpen;
    }

}