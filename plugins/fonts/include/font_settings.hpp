#pragma once

#include <hex/api/content_registry.hpp>

namespace hex::fonts {

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
        FontSelector() : m_fontSize(16, 2, 100), m_bold(false), m_italic(false), m_antiAliased(true) { }

        bool draw(const std::string &name) override;

        nlohmann::json store() override;
        void load(const nlohmann::json& data) override;

        [[nodiscard]] const std::fs::path& getFontPath() const;
        [[nodiscard]] bool isPixelPerfectFont() const;
        [[nodiscard]] float getFontSize() const;
        [[nodiscard]] bool isBold() const;
        [[nodiscard]] bool isItalic() const;
        [[nodiscard]] bool isAntiAliased() const;

    private:
        bool drawPopup();

    private:
        FontFilePicker m_fontFilePicker;
        SliderPoints m_fontSize;
        ContentRegistry::Settings::Widgets::Checkbox m_bold, m_italic, m_antiAliased;

        bool m_applyEnabled = false;
    };

    ContentRegistry::Settings::Widgets::Widget::Interface& addFontSettingsWidget(UnlocalizedString name);

}