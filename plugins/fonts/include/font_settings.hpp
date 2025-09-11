#pragma once

#include <hex/api/imhex_api/system.hpp>
#include <hex/api/imhex_api/fonts.hpp>
#include <hex/api/content_registry/settings.hpp>

namespace hex::fonts {

    enum class AntialiasingType {
        None,
        Grayscale,
        Lcd
    };

    class AntialiasPicker : public ContentRegistry::Settings::Widgets::DropDown {
    public:

        AntialiasPicker() : DropDown(create()) { }

    private:
        static bool isSubpixelRenderingSupported() {
            #if defined(OS_WINDOWS) || defined(OS_LINUX)
                return ImHexApi::System::getGLVersion() >= SemanticVersion(4,1,0);
            #else
                return false;
            #endif
        }

        static DropDown create() {
            if (isSubpixelRenderingSupported()) {
                return DropDown(
                    std::vector<UnlocalizedString>{ "hex.fonts.setting.font.antialias_none", "hex.fonts.setting.font.antialias_grayscale", "hex.fonts.setting.font.antialias_subpixel" },
                    { "none", "grayscale" , "subpixel" },
                    "subpixel"
                );
            } else {
                return DropDown(
                    std::vector<UnlocalizedString>{ "hex.fonts.setting.font.antialias_none", "hex.fonts.setting.font.antialias_grayscale" },
                    { "none", "grayscale" },
                    "grayscale"
                );
            }
        }
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

    private:
        bool m_changed = false;
    };


    class FontSelector : public ContentRegistry::Settings::Widgets::Widget {
    public:
        FontSelector() : m_fontSize(12, 2, 100), m_bold(false), m_italic(false) { }

        bool draw(const std::string &name) override;

        nlohmann::json store() override;
        void load(const nlohmann::json& data) override;

        [[nodiscard]] const std::fs::path& getFontPath() const;
        [[nodiscard]] bool isPixelPerfectFont() const;
        [[nodiscard]] float getFontSize() const;
        [[nodiscard]] bool isBold() const;
        [[nodiscard]] bool isItalic() const;
        [[nodiscard]] AntialiasingType getAntialiasingType() const;

    private:
        FontFilePicker m_fontFilePicker;
        SliderPoints m_fontSize;
        AntialiasPicker m_antiAliased;

        bool m_bold, m_italic;
    };

    ContentRegistry::Settings::Widgets::Widget::Interface& addFontSettingsWidget(UnlocalizedString name);

}