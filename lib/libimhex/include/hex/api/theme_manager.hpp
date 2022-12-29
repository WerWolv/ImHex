#pragma once

#include <hex.hpp>
#include <hex/helpers/fs.hpp>

#include <string>

#include <nlohmann/json_fwd.hpp>
#include <imgui.h>

namespace hex::api {

    class ThemeManager {
    public:
        constexpr static auto NativeTheme = "Native";

        static void changeTheme(std::string name);

        static void addTheme(const std::string &content);
        static void addThemeHandler(const std::string &name, const std::function<void(std::string, std::string)> &handler);
        static void addStyleHandler(const std::string &name, const std::function<void(std::string, std::string)> &handler);

        static std::vector<std::string> getThemeNames();
        static const std::string &getThemeImagePostfix();

        static std::optional<ImColor> parseColorString(const std::string &colorString);
    private:
        ThemeManager() = default;

        static std::map<std::string, nlohmann::json> s_themes;
        static std::map<std::string, std::function<void(std::string, std::string)>> s_themeHandlers, s_styleHandlers;
        static std::string s_imagePostfix;
    };

}