#pragma once

#include <hex/api/content_registry.hpp>

namespace hex::fonts {
    class AntialiasPicker : public ContentRegistry::Settings::Widgets::DropDown {
    public:
        AntialiasPicker() : DropDown(
                std::vector<UnlocalizedString>({"hex.fonts.setting.font.antialias_none", "hex.fonts.setting.font.antialias_grayscale", "hex.fonts.setting.font.antialias_subpixel"}),
                std::vector<nlohmann::json>({"none" , "grayscale" , "subpixel"}),
                nlohmann::json({"subpixel"})
                ){}
    };

    class FontFilePicker : public ContentRegistry::Settings::Widgets::FilePicker {
    public:
        bool draw(const std::string &name) override;

        bool isPixelPerfectFontSelected() const;
        const std::string& getSelectedFontName() const;

        void load(const nlohmann::json& data) override;
        nlohmann::json store() override;

    private:
        bool updateSelectedFontName();

    private:
        std::string m_selectedFontName;
        bool m_pixelPerfectFont = false;
    };

    class SliderPoints : public ContentRegistry::Settings::Widgets::SliderFloat {
    public:
        SliderPoints(float defaultValue, float min, float max) : SliderFloat(defaultValue, min, max) { }
        bool draw(const std::string &name) override;
    };


    class FontSelector : public ContentRegistry::Settings::Widgets::Widget {
    public:
        FontSelector() : m_fontSize(16, 2, 100), m_antiAliased(), m_bold(false), m_italic(false) { }

        bool draw(const std::string &name) override;

        nlohmann::json store() override;
        void load(const nlohmann::json& data) override;

        [[nodiscard]] const std::fs::path& getFontPath() const;
        [[nodiscard]] bool isPixelPerfectFont() const;
        [[nodiscard]] float getFontSize() const;
        [[nodiscard]] bool isBold() const;
        [[nodiscard]] bool isItalic() const;
        [[nodiscard]] const std::string antiAliasingType() const;

    private:
        bool drawPopup();

    private:
        FontFilePicker m_fontFilePicker;
        SliderPoints m_fontSize;
        AntialiasPicker m_antiAliased;
        ContentRegistry::Settings::Widgets::Checkbox m_bold, m_italic;

        bool m_applyEnabled = false;
    };

    ContentRegistry::Settings::Widgets::Widget::Interface& addFontSettingsWidget(UnlocalizedString name);

}