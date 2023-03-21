#pragma once

#include <hex.hpp>
#include <hex/helpers/fs.hpp>

#include <string>
#include <variant>

#include <nlohmann/json_fwd.hpp>
#include <imgui.h>

namespace hex::api {

    /**
     * @brief The Theme Manager takes care of loading and applying themes
     */
    class ThemeManager {
    public:
        constexpr static auto NativeTheme = "Native";

        using ColorMap = std::map<std::string, u32>;

        struct Style {
            std::variant<ImVec2*, float*> value;
            float min;
            float max;
            bool needsScaling;
        };
        using StyleMap = std::map<std::string, Style>;

        /**
         * @brief Changes the current theme to the one with the given name
         * @param name Name of the theme to change to
         */
        static void changeTheme(std::string name);

        /**
         * @brief Adds a theme from json data
         * @param content JSON data of the theme
         */
        static void addTheme(const std::string &content);

        /**
         * @brief Adds a theme handler to handle color values loaded from a theme file
         * @param name Name of the handler
         * @param colorMap Map of color names to their respective constants
         * @param getFunction Function to get the color value of a constant
         * @param setFunction Function to set the color value of a constant
         */
        static void addThemeHandler(const std::string &name, const ColorMap &colorMap, const std::function<ImColor(u32)> &getFunction, const std::function<void(u32, ImColor)> &setFunction);

        /**
         * @brief Adds a style handler to handle style values loaded from a theme file
         * @param name Name of the handler
         * @param styleMap Map of style names to their respective constants
         */
        static void addStyleHandler(const std::string &name, const StyleMap &styleMap);


        static std::vector<std::string> getThemeNames();
        static const std::string &getThemeImagePostfix();

        static std::optional<ImColor> parseColorString(const std::string &colorString);

        static nlohmann::json exportCurrentTheme(const std::string &name);

        static void reset();


    public:
        struct ThemeHandler {
            ColorMap colorMap;
            std::function<ImColor(u32)> getFunction;
            std::function<void(u32, ImColor)> setFunction;
        };

        struct StyleHandler {
            StyleMap styleMap;
        };

        static std::map<std::string, ThemeHandler>& getThemeHandlers() { return s_themeHandlers; }
        static std::map<std::string, StyleHandler>& getStyleHandlers() { return s_styleHandlers; }

    private:
        ThemeManager() = default;


        static std::map<std::string, nlohmann::json> s_themes;
        static std::map<std::string, ThemeHandler> s_themeHandlers;
        static std::map<std::string, StyleHandler> s_styleHandlers;
        static std::string s_imagePostfix;
        static std::string s_currTheme;
    };

}