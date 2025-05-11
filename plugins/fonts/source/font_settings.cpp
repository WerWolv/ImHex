#include <font_settings.hpp>

#include <hex/api/content_registry.hpp>
#include <wolv/utils/string.hpp>
#include <hex/helpers/utils.hpp>

#include <imgui.h>
#include "hex/api/imhex_api.hpp"

namespace hex::fonts {
    constexpr static auto PixelPerfectName = "Pixel-Perfect Default Font (Proggy Clean)";
    constexpr static auto SmoothName = "Smooth Default Font (JetbrainsMono)";
    constexpr static auto CustomName = "Custom Font";

    bool FontFilePicker::draw(const std::string &name) {
        bool changed = false;

        const bool pixelPerfectFont = isPixelPerfectFontSelected();
        bool customFont = updateSelectedFontName();

        if (ImGui::BeginCombo(name.c_str(), m_selectedFontName.c_str())) {
            if (ImGui::Selectable(PixelPerfectName, m_path.empty() && pixelPerfectFont)) {
                m_path.clear();
                m_pixelPerfectFont = true;
                changed = true;
            }

            if (ImGui::Selectable(SmoothName, m_path.empty() && !pixelPerfectFont)) {
                m_path.clear();
                m_pixelPerfectFont = false;
                changed = true;
            }

            if (ImGui::Selectable(CustomName, customFont)) {
                changed = fs::openFileBrowser(fs::DialogMode::Open, { { "TTF Font", "ttf" }, { "OTF Font", "otf" } }, [this](const std::fs::path &path) {
                    m_path = path;
                    m_pixelPerfectFont = false;
                });
            }

            u32 index = 0;
            for (const auto &[path, fontName] : hex::getFonts()) {
                ImGui::PushID(index);
                if (ImGui::Selectable(limitStringLength(fontName, 50).c_str(), m_path == path)) {
                    m_path = path;
                    m_pixelPerfectFont = false;
                    changed = true;
                }
                ImGui::SetItemTooltip("%s", fontName.c_str());
                ImGui::PopID();

                index += 1;
            }

            ImGui::EndCombo();
        }

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
            m_selectedFontName = PixelPerfectName;
        } else if (m_path.empty() && !pixelPerfectFont) {
            m_selectedFontName = SmoothName;
        } else if (fonts.contains(m_path)) {
            m_selectedFontName = fonts.at(m_path);
        } else {
            m_selectedFontName = wolv::util::toUTF8String(m_path.filename());
            customFont = true;
        }

        return customFont;
    }

    bool SliderPoints::draw(const std::string &name) {
        auto scaleFactor = ImHexApi::System::getBackingScaleFactor();
        float value = ImHexApi::Fonts::pixelsToPoints(m_value) * scaleFactor;
        float min = ImHexApi::Fonts::pixelsToPoints(m_min) * scaleFactor;
        float max = ImHexApi::Fonts::pixelsToPoints(m_max) * scaleFactor;

        auto changed = ImGui::SliderFloat(name.c_str(), &value, min, max, "%.0f pt");

        m_value = ImHexApi::Fonts::pointsToPixels(value / scaleFactor);

        return changed;
    }


    bool FontSelector::draw(const std::string &name) {
        ImGui::PushID(name.c_str());
        ON_SCOPE_EXIT { ImGui::PopID(); };

        if (ImGui::Button(m_fontFilePicker.getSelectedFontName().c_str(), ImVec2(300_scaled, 0))) {
            ImGui::OpenPopup("Fonts");
        }

        ImGui::SameLine();

        ImGui::TextUnformatted(name.c_str());

        ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
        return drawPopup();
    }

    nlohmann::json FontSelector::store() {
        nlohmann::json json = nlohmann::json::object();

        json["font_file"] = m_fontFilePicker.store();
        json["font_size"] = m_fontSize.store();
        json["bold"] = m_bold.store();
        json["italic"] = m_italic.store();
        json["antialiased"] = m_antiAliased.store();

        return json;
    }
    void FontSelector::load(const nlohmann::json& data) {
        m_fontFilePicker.load(data["font_file"]);
        m_fontSize.load(data["font_size"]);
        m_bold.load(data["bold"]);
        m_italic.load(data["italic"]);
        m_antiAliased.load(data["antialiased"]);
    }

    bool FontSelector::drawPopup() {
        bool changed = false;
        if (ImGui::BeginPopup("Fonts")) {
            if (m_fontFilePicker.draw("hex.fonts.setting.font.custom_font"_lang)) m_applyEnabled = true;

            ImGui::BeginDisabled(m_fontFilePicker.isPixelPerfectFontSelected());
            {
                if (m_fontSize.draw("hex.fonts.setting.font.font_size"_lang)) m_applyEnabled = true;
                if (m_bold.draw("hex.fonts.setting.font.font_bold"_lang)) m_applyEnabled = true;
                if (m_italic.draw("hex.fonts.setting.font.font_italic"_lang)) m_applyEnabled = true;
                if (m_antiAliased.draw("hex.fonts.setting.font.font_antialias"_lang)) m_applyEnabled = true;
            }
            ImGui::EndDisabled();

            ImGui::NewLine();

            ImGui::BeginDisabled(!m_applyEnabled);
            if (ImGui::Button("hex.ui.common.apply"_lang)) {
                changed = true;
                m_applyEnabled = false;
            }
            ImGui::EndDisabled();

            ImGui::EndPopup();
        }

        return changed;
    }

    [[nodiscard]] const std::fs::path& FontSelector::getFontPath() const {
        return m_fontFilePicker.getPath();
    }

    [[nodiscard]] bool FontSelector::isPixelPerfectFont() const {
        return m_fontFilePicker.isPixelPerfectFontSelected();
    }

    [[nodiscard]] float FontSelector::getFontSize() const {
        return m_fontSize.getValue();
    }

    [[nodiscard]] bool FontSelector::isBold() const {
        return m_bold.isChecked();
    }

    [[nodiscard]] bool FontSelector::isItalic() const {
        return m_italic.isChecked();
    }

    [[nodiscard]] const std::string FontSelector::antiAliasingType() const {
        if (isPixelPerfectFont())
            return "none";
        return m_antiAliased.getValue();
    }


    ContentRegistry::Settings::Widgets::Widget::Interface& addFontSettingsWidget(UnlocalizedString name) {
        return ContentRegistry::Settings::add<FontSelector>("hex.fonts.setting.font", "hex.fonts.setting.font.custom_font", std::move(name));
    }

}
