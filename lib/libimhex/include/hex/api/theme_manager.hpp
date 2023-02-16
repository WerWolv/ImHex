#pragma once

#include <hex.hpp>
#include <hex/helpers/fs.hpp>

#include <string>
#include <variant>

#include <nlohmann/json_fwd.hpp>
#include <imgui.h>

namespace hex::api {

    class ThemeManager {
    public:
        constexpr static auto NativeTheme = "Native";

        using ColorMap = std::map<std::string, u32>;

        struct Style {
            std::variant<ImVec2*, float*> value;
            float min;
            float max;
        };
        using StyleMap = std::map<std::string, Style>;

        static void changeTheme(std::string name);

        static void addTheme(const std::string &content);
        static void addThemeHandler(const std::string &name, const ColorMap &colorMap, const std::function<ImColor(u32)> &getFunction, const std::function<void(u32, ImColor)> &setFunction);
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