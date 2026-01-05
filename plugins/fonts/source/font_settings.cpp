#include <font_settings.hpp>

#include <hex/api/content_registry/settings.hpp>
#include <hex/helpers/utils.hpp>

#include <imgui.h>
#include <fonts/fonts.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/auto_reset.hpp>
#include <hex/ui/imgui_imhex_extensions.h>
#include <romfs/romfs.hpp>

#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

namespace hex::fonts {
    constexpr static auto PixelPerfectFontName = "Pixel-Perfect Default Font (Proggy Clean)";
    constexpr static auto SmoothFontName = "Smooth Default Font (JetBrains Mono)";
    constexpr static auto CustomFontName = "Custom Font";

    static AutoReset<std::map<std::fs::path, ImFont*>> s_previewFonts, s_usedFonts;
    static AutoReset<std::map<std::fs::path, std::string>> s_filteredFonts;
    static bool pushPreviewFont(const std::fs::path &fontPath) {
        if (fontPath.empty()) {
            pushPreviewFont(SmoothFontName);
            return true;
        }

        ImFontConfig config = {};
        config.FontDataOwnedByAtlas = true;
        config.Flags |= ImFontFlags_NoLoadError;

        auto atlas = ImGui::GetIO().Fonts;

        auto it = s_previewFonts->find(fontPath);
        if (it == s_previewFonts->end()) {
            ImFont *font = nullptr;
            if (fontPath == PixelPerfectFontName) {
                font = atlas->AddFontDefault(&config);
            } else if (fontPath == SmoothFontName || fontPath.empty()) {
                static auto jetbrainsFont = romfs::get("fonts/JetBrainsMono.ttf");
                config.FontDataOwnedByAtlas = false;

                font = atlas->AddFontFromMemoryTTF(const_cast<u8 *>(jetbrainsFont.data<u8>()), jetbrainsFont.size(), 0.0F, &config);
            } else {
                font = atlas->AddFontFromFileTTF(wolv::util::toUTF8String(fontPath).c_str(), 0.0F, &config);
            }

            it = s_previewFonts->emplace(fontPath, font).first;
        }

        const auto &[path, font] = *it;
        if (font == nullptr) {
            return false;
        }
        if (!font->IsGlyphInFont(L'A')) {
            // If the font doesn't contain any of the ASCII characters, it's
            // probably not of much use to us
            return false;
        }

        ImGui::PushFont(font, 0.0F);
        s_usedFonts->emplace(path, font);
        return true;
    }

    static void cleanUnusedPreviewFonts() {
        auto atlas = ImGui::GetIO().Fonts;
        for (const auto &[path, font] : *s_previewFonts) {
            if (font == nullptr)
                continue;

            if (!s_usedFonts->contains(path)) {
                atlas->RemoveFont(font);
            }
        }

        s_previewFonts = s_usedFonts;
        s_usedFonts->clear();
    }

    bool FontFilePicker::draw(const std::string &name) {
        bool changed = false;

        const bool pixelPerfectFont = isPixelPerfectFontSelected();
        bool customFont = updateSelectedFontName();

        if (ImGui::BeginCombo(name.c_str(), m_selectedFontName.c_str())) {
            pushPreviewFont(PixelPerfectFontName);
            if (ImGui::Selectable(PixelPerfectFontName, m_path.empty() && pixelPerfectFont)) {
                m_path.clear();
                m_pixelPerfectFont = true;
                changed = true;
            }
            ImGui::PopFont();

            pushPreviewFont(SmoothFontName);
            if (ImGui::Selectable(SmoothFontName, m_path.empty() && !pixelPerfectFont)) {
                m_path.clear();
                m_pixelPerfectFont = false;
                changed = true;
            }
            ImGui::PopFont();

            pushPreviewFont(customFont ? m_path : SmoothFontName);
            if (ImGui::Selectable(CustomFontName, customFont)) {
                changed = fs::openFileBrowser(fs::DialogMode::Open, { { "TTF Font", "ttf" }, { "OTF Font", "otf" } }, [this](const std::fs::path &path) {
                    m_path = path;
                    m_pixelPerfectFont = false;
                });
            }
            ImGui::PopFont();

            if (s_filteredFonts->empty()) {
                for (const auto &[path, fontName] : hex::getFonts()) {
                    if (!pushPreviewFont(path))
                        continue;
                    ImGui::PopFont();
                    s_filteredFonts->emplace(path, fontName);
                }
            }

            {
                u32 index = 0;
                ImGuiListClipper clipper;

                clipper.Begin(s_filteredFonts->size(), ImGui::GetTextLineHeightWithSpacing());

                while (clipper.Step())
                    for (const auto &[path, fontName] : *s_filteredFonts | std::views::drop(clipper.DisplayStart) | std::views::take(clipper.DisplayEnd - clipper.DisplayStart)) {
                        if (!pushPreviewFont(path))
                            continue;

                        ImGui::PushID(index);
                        if (ImGui::Selectable(limitStringLength(fontName, 50).c_str(), m_path == path)) {
                            m_path = path;
                            m_pixelPerfectFont = false;
                            changed = true;
                        }
                        ImGui::PopFont();
                        ImGui::SetItemTooltip("%s", fontName.c_str());
                        ImGui::PopID();

                        index += 1;
                    }

                clipper.SeekCursorForItem(index);
            }

            ImGui::EndCombo();
        }

        TaskManager::doLaterOnce([] {
            cleanUnusedPreviewFonts();
        });

        return changed;
    }

    bool FontFilePicker::isPixelPerfectFontSelected() const {
        return m_pixelPerfectFont;
    }

    const std::string& FontFilePicker::getSelectedFontName() const {
        return m_selectedFontName;
    }

    void FontFilePicker::load(const nlohmann::json& data) {
        FilePicker::load(data["path"]);
        m_pixelPerfectFont = data["pixel_perfect_font"];

        updateSelectedFontName();
    }

    nlohmann::json FontFilePicker::store() {
        nlohmann::json data = nlohmann::json::object();
        data["path"] = FilePicker::store();
        data["pixel_perfect_font"] = m_pixelPerfectFont;

        return data;
    }

    bool FontFilePicker::updateSelectedFontName() {
        const auto &fonts = hex::getFonts();
        bool customFont = false;
        const bool pixelPerfectFont = isPixelPerfectFontSelected();

        if (m_path.empty() && pixelPerfectFont) {
            m_selectedFontName = PixelPerfectFontName;
        } else if (m_path.empty() && !pixelPerfectFont) {
            m_selectedFontName = SmoothFontName;
        } else if (fonts.contains(m_path)) {
            m_selectedFontName = fonts.at(m_path);
        } else {
            m_selectedFontName = wolv::util::toUTF8String(m_path.filename());
            customFont = true;
        }

        return customFont;
    }

    bool SliderPoints::draw(const std::string &name) {
        if (ImGui::SliderFloat(name.c_str(), &m_value, m_min, m_max, "%.0f pt"))
            m_changed = true;

        if (m_changed && !ImGui::IsItemActive()) {
            m_changed = false;
            return true;
        } else {
            return false;
        }
    }


    bool FontSelector::draw(const std::string &name) {
        ImGui::PushID(name.c_str());
        ON_SCOPE_EXIT { ImGui::PopID(); };

        bool changed = false;
        if (ImGui::CollapsingHeader(name.c_str())) {
            if (ImGuiExt::BeginBox()) {
                if (m_fontFilePicker.draw("hex.fonts.setting.font.custom_font"_lang)) changed = true;

                ImGui::BeginDisabled(m_fontFilePicker.isPixelPerfectFontSelected());
                {
                    if (m_fontSize.draw("hex.fonts.setting.font.font_size"_lang))
                        changed = true;

                    const auto buttonHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y;

                    fonts::Default().pushBold();
                    if (ImGuiExt::DimmedButtonToggle("hex.fonts.setting.font.button.bold"_lang, &m_bold, ImVec2(buttonHeight, buttonHeight)))
                        changed = true;
                    fonts::Default().pop();
                    ImGui::SetItemTooltip("%s", "hex.fonts.setting.font.font_bold"_lang.get());

                    ImGui::SameLine();

                    fonts::Default().pushItalic();
                    if (ImGuiExt::DimmedButtonToggle("hex.fonts.setting.font.button.italic"_lang, &m_italic, ImVec2(buttonHeight, buttonHeight)))
                        changed = true;
                    fonts::Default().pop();
                    ImGui::SetItemTooltip("%s", "hex.fonts.setting.font.font_italic"_lang.get());

                    if (m_antiAliased.draw("hex.fonts.setting.font.font_antialias"_lang))
                        changed = true;
                }
                ImGui::EndDisabled();

                ImGuiExt::EndBox();
            }
        }

        return changed;
    }

    nlohmann::json FontSelector::store() {
        nlohmann::json json = nlohmann::json::object();

        json["font_file"] = m_fontFilePicker.store();
        json["font_size_pt"] = m_fontSize.store();
        json["bold"] = m_bold;
        json["italic"] = m_italic;
        json["antialiased"] = m_antiAliased.store();

        return json;
    }
    void FontSelector::load(const nlohmann::json& data) {
        if (data.contains("font_file"))
            m_fontFilePicker.load(data["font_file"]);
        if (data.contains("font_size_pt"))
            m_fontSize.load(data["font_size_pt"]);
        if (data.contains("bold"))
            m_bold = data["bold"];
        if (data.contains("italic"))
            m_italic = data["italic"];
        if (data.contains("antialiased"))
            m_antiAliased.load(data["antialiased"]);
    }

    [[nodiscard]] const std::fs::path& FontSelector::getFontPath() const {
        return m_fontFilePicker.getPath();
    }

    [[nodiscard]] bool FontSelector::isPixelPerfectFont() const {
        return m_fontFilePicker.isPixelPerfectFontSelected();
    }

    [[nodiscard]] float FontSelector::getFontSize() const {
        return ImHexApi::Fonts::pointsToPixels(m_fontSize.getValue());
    }

    [[nodiscard]] bool FontSelector::isBold() const {
        return m_bold;
    }

    [[nodiscard]] bool FontSelector::isItalic() const {
        return m_italic;
    }

    [[nodiscard]] AntialiasingType FontSelector::getAntialiasingType() const {
        if (isPixelPerfectFont())
            return AntialiasingType::None;

        auto value = m_antiAliased.getValue();
        if (value == "none")
            return AntialiasingType::None;
        else if (value == "grayscale")
            return AntialiasingType::Grayscale;
        else if (value == "subpixel")
            return AntialiasingType::Lcd;
        else
            return AntialiasingType::Grayscale;

    }


    ContentRegistry::Settings::Widgets::Widget::Interface& addFontSettingsWidget(UnlocalizedString name) {
        return ContentRegistry::Settings::add<FontSelector>("hex.fonts.setting.font", "hex.fonts.setting.font.custom_font", name);
    }

}
