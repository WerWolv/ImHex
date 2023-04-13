#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/localization.hpp>
#include <hex/api/theme_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/logger.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <fonts/codicons_font.h>

#include <nlohmann/json.hpp>

#include <ui/pattern_drawer.hpp>

#include <wolv/utils/guards.hpp>

namespace {

    std::vector<std::fs::path> userFolders;

    void loadUserFoldersFromSetting(const std::vector<std::string> &paths) {
        userFolders.clear();
        for (const auto &path : paths) {
            userFolders.emplace_back(
                reinterpret_cast<const char8_t*>(path.data()),
                reinterpret_cast<const char8_t*>(path.data() + path.size())
            );
        }
    }

}

namespace hex::plugin::builtin {

    void registerSettings() {

        /* General */

        /* Values of this setting :
        0 - do not check for updates on startup
        1 - check for updates on startup
        2 - default value - ask the user if he wants to check for updates. This value should only be encountered on the first startup.
        */
        ContentRegistry::Settings::add("hex.builtin.setting.general", "hex.builtin.setting.general.check_for_updates", 2, [](auto name, nlohmann::json &setting) {
            static bool enabled = static_cast<int>(setting) == 1;

            if (ImGui::Checkbox(name.data(), &enabled)) {
                setting = static_cast<int>(enabled);
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.general", "hex.builtin.setting.general.show_tips", 1, [](auto name, nlohmann::json &setting) {
            static bool enabled = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &enabled)) {
                setting = static_cast<int>(enabled);
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.general", "hex.builtin.setting.general.auto_load_patterns", 1, [](auto name, nlohmann::json &setting) {
            static bool enabled = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &enabled)) {
                setting = static_cast<int>(enabled);
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.general", "hex.builtin.setting.general.sync_pattern_source", 0, [](auto name, nlohmann::json &setting) {
            static bool enabled = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &enabled)) {
                setting = static_cast<int>(enabled);
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.general", "hex.builtin.setting.general.enable_unicode", 1, [](auto name, nlohmann::json &setting) {
            static bool enabled = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &enabled)) {
                setting = static_cast<int>(enabled);
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.general", "hex.builtin.setting.general.save_recent_providers", 1, [](auto name, nlohmann::json &setting) {
            static bool enabled = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &enabled)) {
                setting = static_cast<int>(enabled);
                return true;
            }

            return false;
        });

        /* Interface */

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.color", "Dark", [](auto name, nlohmann::json &setting) {
            static auto selection = static_cast<std::string>(setting);

            const auto themeNames = ThemeManager::getThemeNames();
            bool changed = false;

            if (ImGui::BeginCombo(name.data(), selection.c_str())) {
                if (ImGui::Selectable(ThemeManager::NativeTheme, selection == ThemeManager::NativeTheme)) {
                    selection = ThemeManager::NativeTheme;
                    setting = selection;
                    ImHexApi::System::enableSystemThemeDetection(true);
                    changed = true;
                }

                for (const auto &themeName : themeNames) {
                    if (ImGui::Selectable(themeName.c_str(), selection == themeName)) {
                        selection = themeName;
                        setting = selection;
                        ImHexApi::System::enableSystemThemeDetection(false);
                        ThemeManager::changeTheme(selection);
                        changed = true;
                    }
                }

                ImGui::EndCombo();
            }

            return changed;
        });

        ContentRegistry::Settings::add(
            "hex.builtin.setting.interface", "hex.builtin.setting.interface.scaling", 0, [](auto name, nlohmann::json &setting) {
                static int selection = static_cast<int>(setting);

                const char *scaling[] = {
                    "hex.builtin.setting.interface.scaling.native"_lang,
                    "hex.builtin.setting.interface.scaling.x0_5"_lang,
                    "hex.builtin.setting.interface.scaling.x1_0"_lang,
                    "hex.builtin.setting.interface.scaling.x1_5"_lang,
                    "hex.builtin.setting.interface.scaling.x2_0"_lang,
                };

                if (ImGui::Combo(name.data(), &selection, scaling, IM_ARRAYSIZE(scaling))) {
                    setting = selection;

                    ImHexApi::System::restartImHex();

                    return true;
                }

                return false;
            },
            true);

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.language", "en-US", [](auto name, nlohmann::json &setting) {
            auto &languages = LangEntry::getSupportedLanguages();

            static int selection = [&]() -> int {
                u16 index = 0;
                for (auto &[languageCode, languageName] : languages) {
                    if (setting.get<std::string>() == languageCode)
                        return index;
                    index++;
                }

                return 0;
            }();

            static auto languageNames = [&]() {
                std::vector<const char *> result;
                result.reserve(languages.size());

                for (auto &[languageCode, languageName] : languages)
                    result.push_back(languageName.c_str());

                return result;
            }();


            if (ImGui::Combo(name.data(), &selection, languageNames.data(), languageNames.size())) {

                u16 index = 0;
                for (auto &[languageCode, languageName] : languages) {
                    if (selection == index) {
                        setting = languageCode;
                        break;
                    }
                    index++;
                }

                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.wiki_explain_language", "en", [](auto name, nlohmann::json &setting) {
            static auto lang = std::string(setting);

            if (ImGui::InputText(name.data(), lang, ImGuiInputTextFlags_CharsNoBlank)) {
                setting = std::string(lang.c_str()); // remove following zero bytes
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.fps", 60, [](auto name, nlohmann::json &setting) {
            static int fps = static_cast<int>(setting);

            auto format = [] -> std::string {
                if (fps > 200)
                    return "hex.builtin.setting.interface.fps.unlocked"_lang;
                else if (fps < 15)
                    return "hex.builtin.setting.interface.fps.native"_lang;
                else
                    return "%d FPS";
            }();

            if (ImGui::SliderInt(name.data(), &fps, 14, 201, format.c_str(), ImGuiSliderFlags_AlwaysClamp)) {
                setting = fps;
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.multi_windows", 1, [](auto name, nlohmann::json &setting) {
            static bool enabled = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &enabled)) {
                setting = static_cast<int>(enabled);
                return true;
            }

            return false;
        }, true);

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.pattern_tree_style", 0, [](auto name, nlohmann::json &setting) {
            static int selection = static_cast<int>(setting);

            const char *style[] = {
                    "hex.builtin.setting.interface.pattern_tree_style.tree"_lang,
                    "hex.builtin.setting.interface.pattern_tree_style.auto_expanded"_lang,
                    "hex.builtin.setting.interface.pattern_tree_style.flattened"_lang,
            };

            if (ImGui::Combo(name.data(), &selection, style, IM_ARRAYSIZE(style))) {
                setting = selection;
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.highlight_color", 0x60C08080, [](auto name, nlohmann::json &setting) {
            static auto color = static_cast<color_t>(setting);

            std::array<float, 4> colorArray = {
                ((color >>  0) & 0x000000FF) / float(0xFF),
                ((color >>  8) & 0x000000FF) / float(0xFF),
                ((color >> 16) & 0x000000FF) / float(0xFF),
                ((color >> 24) & 0x000000FF) / float(0xFF)
            };

            if (ImGui::ColorEdit4(name.data(), colorArray.data(), ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoInputs)) {
                color =
                    (color_t(colorArray[0] * 0xFF) <<  0) |
                    (color_t(colorArray[1] * 0xFF) <<  8) |
                    (color_t(colorArray[2] * 0xFF) << 16) |
                    (color_t(colorArray[3] * 0xFF) << 24);

                setting = color;

                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.bytes_per_row", 16, [](auto name, nlohmann::json &setting) {
            static int columns = static_cast<int>(setting);

            if (ImGui::SliderInt(name.data(), &columns, 1, 32)) {
                setting = columns;
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.ascii", 1, [](auto name, nlohmann::json &setting) {
            static bool ascii = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &ascii)) {
                setting = static_cast<int>(ascii);
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.advanced_decoding", 1, [](auto name, nlohmann::json &setting) {
            static bool advancedDecoding = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &advancedDecoding)) {
                setting = static_cast<int>(advancedDecoding);
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.grey_zeros", 1, [](auto name, nlohmann::json &setting) {
            static bool greyZeros = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &greyZeros)) {
                setting = static_cast<int>(greyZeros);
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.uppercase_hex", 1, [](auto name, nlohmann::json &setting) {
            static bool upperCaseHex = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &upperCaseHex)) {
                setting = static_cast<int>(upperCaseHex);
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.visualizer", "hex.builtin.visualizer.hexadecimal.8bit", [](auto name, nlohmann::json &setting) {
            auto &visualizers = ContentRegistry::HexEditor::impl::getVisualizers();

            auto selectedVisualizer = setting;

            bool result = false;
            if (ImGui::BeginCombo(name.data(), LangEntry(selectedVisualizer))) {

                for (const auto &[unlocalizedName, visualizer] : visualizers) {
                    if (ImGui::Selectable(LangEntry(unlocalizedName))) {
                        setting = unlocalizedName;
                        result  = true;
                    }
                }

                ImGui::EndCombo();
            }

            return result;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.sync_scrolling", 0, [](auto name, nlohmann::json &setting) {
            static bool syncScrolling = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &syncScrolling)) {
                setting = static_cast<int>(syncScrolling);
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.byte_padding", 0, [](auto name, nlohmann::json &setting) {
            static int padding = static_cast<int>(setting);

            if (ImGui::SliderInt(name.data(), &padding, 0, 50)) {
                setting = padding;
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.char_padding", 0, [](auto name, nlohmann::json &setting) {
            static int padding = static_cast<int>(setting);

            if (ImGui::SliderInt(name.data(), &padding, 0, 50)) {
                setting = padding;
                return true;
            }

            return false;
        });


        /* Fonts */

        static std::string fontPath;
        ContentRegistry::Settings::add(
            "hex.builtin.setting.font", "hex.builtin.setting.font.font_path", "", [](auto name, nlohmann::json &setting) {
                fontPath = static_cast<std::string>(setting);

                if (ImGui::InputText("##font_path", fontPath)) {
                    setting = fontPath;
                    return true;
                }

                ImGui::SameLine();

                if (ImGui::IconButton(ICON_VS_FOLDER_OPENED, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    return fs::openFileBrowser(fs::DialogMode::Open, { { "TTF Font", "ttf" }, { "OTF Font", "otf" } },
                        [&](const std::fs::path &path) {
                            fontPath = wolv::util::toUTF8String(path);
                            setting  = fontPath;
                        });
                }

                ImGui::SameLine();

                ImGui::TextFormatted("{}", name);

                return false;
            },
            true);

        ContentRegistry::Settings::add(
            "hex.builtin.setting.font", "hex.builtin.setting.font.font_size", 13, [](auto name, nlohmann::json &setting) {
                static int fontSize = static_cast<int>(setting);

                ImGui::BeginDisabled(fontPath.empty());
                ON_SCOPE_EXIT { ImGui::EndDisabled(); };

                if (ImGui::SliderInt(name.data(), &fontSize, 0, 100, "%d", ImGuiSliderFlags_NoInput)) {
                    setting = fontSize;
                    return true;
                }

                if (fontPath.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetNextWindowSize(scaled(ImVec2(300, 0)));
                    ImGui::BeginTooltip();
                    ImGui::TextFormattedWrapped("{}", "hex.builtin.setting.font.font_size.tooltip"_lang);
                    ImGui::EndTooltip();
                }

                return false;
            },
            true);


        /* Folders */

        static const std::string dirsSetting { "hex.builtin.setting.folders" };

        ContentRegistry::Settings::addCategoryDescription(dirsSetting, "hex.builtin.setting.folders.description");

        ContentRegistry::Settings::add(dirsSetting, dirsSetting, std::vector<std::string> {}, [](auto name, nlohmann::json &setting) {
            hex::unused(name);

            static size_t currentItemIndex = [&setting] {loadUserFoldersFromSetting(setting); return 0; }();

            auto saveToSetting = [&setting] {
                std::vector<std::string> folderStrings;
                for (const auto &folder : userFolders) {
                    auto utfString = folder.u8string();
                    // JSON stores char8_t as array, char8_t is not supported as of now
                    folderStrings.emplace_back(reinterpret_cast<const char *>(&utfString.front()), reinterpret_cast<const char *>(std::next(&utfString.back())));
                }
                setting = folderStrings;
                ImHexApi::System::setAdditionalFolderPaths(userFolders);
            };

            bool result = false;

            if (!ImGui::BeginListBox("", ImVec2(-38, -FLT_MIN))) {
                return false;
            } else {
                for (size_t n = 0; n < userFolders.size(); n++) {
                    const bool isSelected = (currentItemIndex == n);
                    if (ImGui::Selectable(wolv::util::toUTF8String(userFolders.at(n)).c_str(), isSelected)) { currentItemIndex = n; }
                    if (isSelected) { ImGui::SetItemDefaultFocus(); }
                }
                ImGui::EndListBox();
            }
            ImGui::SameLine();
            ImGui::BeginGroup();

            if (ImGui::IconButton(ICON_VS_NEW_FOLDER, ImGui::GetCustomColorVec4(ImGuiCustomCol_DescButton), ImVec2(30, 30))) {
                fs::openFileBrowser(fs::DialogMode::Folder, {}, [&](const std::fs::path &path) {
                    if (std::find(userFolders.begin(), userFolders.end(), path) == userFolders.end()) {
                        userFolders.emplace_back(path);
                        saveToSetting();
                        result = true;
                    }
                });
            }
            ImGui::InfoTooltip("hex.builtin.setting.folders.add_folder"_lang);

            if (ImGui::IconButton(ICON_VS_REMOVE_CLOSE, ImGui::GetCustomColorVec4(ImGuiCustomCol_DescButton), ImVec2(30, 30))) {
                if (!userFolders.empty()) {
                    userFolders.erase(std::next(userFolders.begin(), currentItemIndex));
                    saveToSetting();

                    result = true;
                }
            }
            ImGui::InfoTooltip("hex.builtin.setting.folders.remove_folder"_lang);

            ImGui::EndGroup();

            return result;
        });

        /* Proxy */

        static const std::string proxySetting { "hex.builtin.setting.proxy" };

        HttpRequest::setProxy(ContentRegistry::Settings::read(proxySetting, "hex.builtin.setting.proxy.url", ""));

        ContentRegistry::Settings::addCategoryDescription(proxySetting, "hex.builtin.setting.proxy.description");

        ContentRegistry::Settings::add(
            proxySetting, "hex.builtin.setting.proxy.url", "", [](auto name, nlohmann::json &setting) {
                static std::string proxyUrl = static_cast<std::string>(setting);
                static bool enableProxy     = !proxyUrl.empty();

                bool result = false;

                if (ImGui::Checkbox("hex.builtin.setting.proxy.enable"_lang, &enableProxy)) {
                    setting = enableProxy ? proxyUrl : "";
                    HttpRequest::setProxy(enableProxy ? proxyUrl : "");
                    result = true;
                }

                ImGui::BeginDisabled(!enableProxy);
                if (ImGui::InputText("##proxy_url", proxyUrl)) {
                    setting = proxyUrl;
                    HttpRequest::setProxy(proxyUrl);
                    result = true;
                }
                ImGui::EndDisabled();

                ImGui::InfoTooltip("hex.builtin.setting.proxy.url.tooltip"_lang);

                ImGui::SameLine();

                ImGui::TextFormatted("{}", name);
                return result;
            },
            false);
    }


    static void loadInterfaceScalingSetting() {
        float interfaceScaling = 1.0F;
        switch (ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.scaling", 0)) {
            default:
            case 0:
                interfaceScaling = ImHexApi::System::getNativeScale();
                break;
            case 1:
                interfaceScaling = 0.5F;
                break;
            case 2:
                interfaceScaling = 1.0F;
                break;
            case 3:
                interfaceScaling = 1.5F;
                break;
            case 4:
                interfaceScaling = 2.0F;
                break;
            case 5:
                interfaceScaling = 3.0F;
                break;
            case 6:
                interfaceScaling = 4.0F;
                break;
        }

        ImHexApi::System::impl::setGlobalScale(interfaceScaling);
    }

    static void loadFontSettings() {
        std::fs::path fontFile = ContentRegistry::Settings::read("hex.builtin.setting.font", "hex.builtin.setting.font.font_path", "");
        if (!wolv::io::fs::exists(fontFile) || !wolv::io::fs::isRegularFile(fontFile))
            fontFile.clear();

        // If no custom font has been specified, search for a file called "font.ttf" in one of the resource folders
        if (fontFile.empty()) {
            for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Resources)) {
                auto path = dir / "font.ttf";
                if (wolv::io::fs::exists(path)) {
                    log::info("Loading custom front from {}", wolv::util::toUTF8String(path));

                    fontFile = path;
                    break;
                }
            }
        }

        ImHexApi::System::impl::setCustomFontPath(fontFile);

        // If a custom font has been loaded now, also load the font size
        float fontSize = ImHexApi::System::DefaultFontSize * std::round(ImHexApi::System::getGlobalScale());
        if (!fontFile.empty()) {
            fontSize = ContentRegistry::Settings::read("hex.builtin.setting.font", "hex.builtin.setting.font.font_size", 13) * ImHexApi::System::getGlobalScale();
        }

        ImHexApi::System::impl::setFontSize(fontSize);
    }

    static void loadThemeSettings() {
        auto theme = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.color", ThemeManager::NativeTheme);

        if (theme == ThemeManager::NativeTheme)
            ImHexApi::System::enableSystemThemeDetection(true);
        else {
            ImHexApi::System::enableSystemThemeDetection(false);
            ThemeManager::changeTheme(theme);
        }
    }

    static void loadFoldersSettings() {
        auto directories = ContentRegistry::Settings::read("hex.builtin.setting.folders", "hex.builtin.setting.folders", std::vector<std::string> { });

        loadUserFoldersFromSetting(directories);
        ImHexApi::System::setAdditionalFolderPaths(userFolders);
    }

    void loadSettings() {
        loadInterfaceScalingSetting();
        loadFontSettings();
        loadThemeSettings();
        loadFoldersSettings();
    }

}
