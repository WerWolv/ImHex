#include <hex/api/theme_manager.hpp>
#include <hex/api/event_manager.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/auto_reset.hpp>

#include <nlohmann/json.hpp>

namespace hex {

    namespace {

        AutoReset<std::map<std::string, nlohmann::json>> s_themes;
        AutoReset<std::map<std::string, ThemeManager::ThemeHandler>> s_themeHandlers;
        AutoReset<std::map<std::string, ThemeManager::StyleHandler>> s_styleHandlers;
        AutoReset<std::string> s_imageTheme;
        AutoReset<std::string> s_currTheme;
        AutoReset<std::optional<float>> s_accentColor;

        std::recursive_mutex s_themeMutex;
    }


    void ThemeManager::reapplyCurrentTheme() {
        ThemeManager::changeTheme(s_currTheme);
    }


    void ThemeManager::addThemeHandler(const std::string &name, const ColorMap &colorMap, const std::function<ImColor(u32)> &getFunction, const std::function<void(u32, ImColor)> &setFunction) {
        std::unique_lock lock(s_themeMutex);

        (*s_themeHandlers)[name] = { colorMap, getFunction, setFunction };
    }

    void ThemeManager::addStyleHandler(const std::string &name, const StyleMap &styleMap) {
        std::unique_lock lock(s_themeMutex);

        (*s_styleHandlers)[name] = { styleMap };
    }

    void ThemeManager::addTheme(const std::string &content) {
        std::unique_lock lock(s_themeMutex);

        try {
            auto theme = nlohmann::json::parse(content);

            if (theme.contains("name") && theme.contains("colors")) {
                (*s_themes)[theme["name"].get<std::string>()] = theme;
            } else {
                hex::log::error("Invalid theme file");
            }
        } catch (const nlohmann::json::parse_error &e) {
            hex::log::error("Invalid theme file: {}", e.what());
        }
    }

    std::optional<ImColor> ThemeManager::parseColorString(const std::string &colorString) {
        if (colorString == "auto")
            return ImVec4(0, 0, 0, -1);

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

        if (color == 0x00000000)
            return ImVec4(0, 0, 0, -1);

        return ImColor(hex::changeEndianness(color, std::endian::big));
    }

    nlohmann::json ThemeManager::exportCurrentTheme(const std::string &name) {
        nlohmann::json theme = {
                { "name", name },
                { "image_theme", s_imageTheme },
                { "colors", {} },
                { "styles", {} },
                { "base", s_currTheme }
        };

        for (const auto &[type, handler] : *s_themeHandlers) {
            theme["colors"][type] = {};

            for (const auto &[key, value] : handler.colorMap) {
                auto color = handler.getFunction(value);
                theme["colors"][type][key] = fmt::format("#{:08X}", hex::changeEndianness(u32(color), std::endian::big));
            }
        }

        for (const auto &[type, handler] : *s_styleHandlers) {
            theme["styles"][type] = {};

            for (const auto &[key, style] : handler.styleMap) {
                if (std::holds_alternative<float*>(style.value)) {
                    theme["styles"][type][key] = *std::get<float*>(style.value);
                } else if (std::holds_alternative<ImVec2*>(style.value)) {
                    theme["styles"][type][key] = {
                            std::get<ImVec2*>(style.value)->x,
                            std::get<ImVec2*>(style.value)->y
                    };
                }
            }
        }

        return theme;
    }

    void ThemeManager::changeTheme(std::string name) {
        std::unique_lock lock(s_themeMutex);

        if (!s_themes->contains(name)) {
            if (s_themes->empty()) {
                return;
            } else {
                const std::string &defaultTheme = s_themes->begin()->first;
                hex::log::error("Theme '{}' does not exist, using default theme '{}' instead!", name, defaultTheme);
                name = defaultTheme;
            }
        }

        const auto &theme = (*s_themes)[name];

        if (theme.contains("base")) {
            if (theme["base"].is_string()) {
                if (theme["base"] != name)
                    changeTheme(theme["base"].get<std::string>());
            } else {
                hex::log::error("Theme '{}' has invalid base theme!", name);
            }
        }

        if (theme.contains("colors") && !s_themeHandlers->empty()) {
            for (const auto&[type, content] : theme["colors"].items()) {
                if (!s_themeHandlers->contains(type)) {
                    log::warn("No theme handler found for '{}'", type);
                    continue;
                }

                const auto &handler = (*s_themeHandlers)[type];
                for (const auto &[key, value] : content.items()) {
                    if (!handler.colorMap.contains(key)) {
                        log::warn("No color found for '{}.{}'", type, key);
                        continue;
                    }

                    auto colorString = value.get<std::string>();
                    bool accentableColor = false;
                    if (colorString.starts_with("*")) {
                        colorString = colorString.substr(1);
                        accentableColor = true;
                    }
                    auto color = parseColorString(colorString);

                    if (!color.has_value()) {
                        log::warn("Invalid color '{}' for '{}.{}'", colorString, type, key);
                        continue;
                    }

                    if (accentableColor && s_accentColor->has_value()) {
                        float h, s, v;
                        ImGui::ColorConvertRGBtoHSV(color->Value.x, color->Value.y, color->Value.z, h, s, v);

                        h = s_accentColor->value();

                        ImGui::ColorConvertHSVtoRGB(h, s, v, color->Value.x, color->Value.y, color->Value.z);
                    }

                    (*s_themeHandlers)[type].setFunction((*s_themeHandlers)[type].colorMap.at(key), color.value());
                }
            }
        }

        if (theme.contains("styles") && !s_styleHandlers->empty()) {
            for (const auto&[type, content] : theme["styles"].items()) {
                if (!s_styleHandlers->contains(type)) {
                    log::warn("No style handler found for '{}'", type);
                    continue;
                }

                auto &handler = (*s_styleHandlers)[type];
                for (const auto &[key, value] : content.items()) {
                    if (!handler.styleMap.contains(key))
                        continue;

                    auto &style = handler.styleMap.at(key);
                    const float scale = style.needsScaling ? 1_scaled : 1.0F;

                    if (value.is_number_float()) {
                        if (const auto newValue = std::get_if<float*>(&style.value); newValue != nullptr && *newValue != nullptr)
                            **newValue = value.get<float>() * scale;
                        else
                            log::warn("Style variable '{}' was of type ImVec2 but a float was expected.", name);
                    } else if (value.is_array() && value.size() == 2 && value[0].is_number_float() && value[1].is_number_float()) {
                        if (const auto newValue = std::get_if<ImVec2*>(&style.value); newValue != nullptr && *newValue != nullptr)
                            **newValue = ImVec2(value[0].get<float>() * scale, value[1].get<float>() * scale);
                        else
                            log::warn("Style variable '{}' was of type float but a ImVec2 was expected.", name);
                    } else {
                        hex::log::error("Theme '{}' has invalid style value for '{}.{}'!", name, type, key);
                    }
                }
            }
        }

        if (theme.contains("image_theme")) {
            if (theme["image_theme"].is_string()) {
                s_imageTheme = theme["image_theme"].get<std::string>();
            } else {
                hex::log::error("Theme '{}' has invalid image theme!", name);
                s_imageTheme = "dark";
            }
        } else {
            s_imageTheme = "dark";
        }

        s_currTheme = name;

        EventThemeChanged::post();
    }

    const std::string &ThemeManager::getImageTheme() {
        return s_imageTheme;
    }

    std::vector<std::string> ThemeManager::getThemeNames() {
        std::vector<std::string> themeNames;
        for (const auto &[name, theme] : *s_themes)
            themeNames.push_back(name);

        return themeNames;
    }

    void ThemeManager::reset() {
        std::unique_lock lock(s_themeMutex);

        s_themes->clear();
        s_styleHandlers->clear();
        s_themeHandlers->clear();
        s_imageTheme->clear();
        s_currTheme->clear();
    }

    void ThemeManager::setAccentColor(const ImColor &color) {
        float h, s, v;
        ImGui::ColorConvertRGBtoHSV(color.Value.x, color.Value.y, color.Value.z, h, s, v);

        s_accentColor = h;
        reapplyCurrentTheme();
    }


    const std::map<std::string, ThemeManager::ThemeHandler> &ThemeManager::getThemeHandlers() {
        return s_themeHandlers;
    }

    const std::map<std::string, ThemeManager::StyleHandler> &ThemeManager::getStyleHandlers() {
        return s_styleHandlers;
    }

}