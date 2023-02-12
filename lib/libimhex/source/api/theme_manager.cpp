#include <hex/api/theme_manager.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/utils.hpp>

#include <nlohmann/json.hpp>

namespace hex::api {

    std::map<std::string, nlohmann::json> ThemeManager::s_themes;
    std::map<std::string, std::function<void(std::string, std::string)>> ThemeManager::s_themeHandlers, ThemeManager::s_styleHandlers;
    std::string ThemeManager::s_imagePostfix;

    void ThemeManager::addThemeHandler(const std::string &name, const std::function<void(std::string, std::string)> &handler) {
        s_themeHandlers[name] = handler;
    }

    void ThemeManager::addStyleHandler(const std::string &name, const std::function<void(std::string, std::string)> &handler) {
        s_styleHandlers[name] = handler;
    }

    void ThemeManager::addTheme(const std::string &content) {
        auto theme = nlohmann::json::parse(content);
        if (theme.contains("name") && theme.contains("colors")) {
            s_themes[theme["name"].get<std::string>()] = theme;
        } else {
            hex::log::error("Invalid theme file");
        }
    }

    std::optional<ImColor> ThemeManager::parseColorString(const std::string &colorString) {
        if (colorString.length() != 9 || colorString[0] != '#')
            return std::nullopt;

        u32 color = 0;

        for (u32 i = 1; i < 9; i++) {
            color <<= 4;
            if (colorString[i] >= '0' && colorString[i] <= '9')
                color |= colorString[i] - '0';
            else if (colorString[i] >= 'A' && colorString[i] <= 'F')
                color |= colorString[i] - 'A' + 10;
            else if (colorString[i] >= 'a' && colorString[i] <= 'f')
                color |= colorString[i] - 'a' + 10;
            else
                return std::nullopt;
        }

        return ImColor(hex::changeEndianess(color, std::endian::big));
    }

    void ThemeManager::changeTheme(std::string name) {
        if (!s_themes.contains(name)) {
            if (s_themes.empty()) {
                return;
            } else {
                const std::string &defaultTheme = s_themes.begin()->first;
                hex::log::error("Theme '{}' does not exist, using default theme '{}' instead!", name, defaultTheme);
                name = defaultTheme;
            }
        }

        const auto &theme = s_themes[name];

        if (theme.contains("base")) {
            if (theme["base"].is_string()) {
                changeTheme(theme["base"].get<std::string>());
            } else {
                hex::log::error("Theme '{}' has invalid base theme!", name);
            }
        }

        if (theme.contains("colors")) {
            for (const auto&[type, content] : theme["colors"].items()) {
                if (!s_themeHandlers.contains(type)) {
                    log::warn("No theme handler found for '{}'", type);
                    continue;
                }

                for (const auto &[key, value] : content.items())
                    s_themeHandlers[type](key, value.get<std::string>());
            }
        }

        if (theme.contains("styles")) {
            for (const auto&[key, value] : theme["styles"].items()) {
                if (!s_styleHandlers.contains(key)) {
                    log::warn("No style handler found for '{}'", key);
                    continue;
                }

                s_styleHandlers[key](name, value.get<std::string>());
            }
        }

        if (theme.contains("image_postfix")) {
            if (theme["image_postfix"].is_string()) {
                s_imagePostfix = theme["image_postfix"].get<std::string>();
            } else {
                hex::log::error("Theme '{}' has invalid image postfix!", name);
            }
        }
    }

    const std::string &ThemeManager::getThemeImagePostfix() {
        return s_imagePostfix;
    }

    std::vector<std::string> ThemeManager::getThemeNames() {
        std::vector<std::string> themeNames;
        for (const auto &[name, theme] : s_themes)
            themeNames.push_back(name);

        return themeNames;
    }

    void ThemeManager::reset() {
        ThemeManager::s_themes.clear();
        ThemeManager::s_styleHandlers.clear();
        ThemeManager::s_themeHandlers.clear();
        ThemeManager::s_imagePostfix.clear();
    }

}