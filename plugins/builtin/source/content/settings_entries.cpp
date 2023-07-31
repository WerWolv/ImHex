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

        ContentRegistry::Settings::add("hex.builtin.setting.general", "hex.builtin.setting.general.load_all_unicode_chars", 0, [](auto name, nlohmann::json &setting) {
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

        ContentRegistry::Settings::add("hex.builtin.setting.general", "hex.builtin.setting.general.network_interface", 0, [](auto name, nlohmann::json &setting) {
            static bool enabled = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &enabled)) {
                setting = static_cast<int>(enabled);
                return true;
            }

            return false;
        });

        /*
            Values of this setting :
            0 - do not check for updates on startup
            1 - check for updates on startup
            2 - default value - ask the user if he wants to check for updates. This value should only be encountered on the first startup.
        */
        ContentRegistry::Settings::add("hex.builtin.setting.general", "hex.builtin.setting.general.server_contact", 2, [](auto name, nlohmann::json &setting) {
            static bool enabled = static_cast<int>(setting) == 1;

            if (ImGui::Checkbox(name.data(), &enabled)) {
                setting = static_cast<int>(enabled);
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.general", "hex.builtin.setting.general.upload_crash_logs", 1, [](auto name, nlohmann::json &setting) {
            static bool enabled = static_cast<int>(setting) == 1;

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
                    return true;
                }

                return false;
            },
            true);

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.language", "en-US", [](auto name, nlohmann::json &setting) {
            auto &languages = LangEntry::getSupportedLanguages();
            if (!languages.contains(setting.get<std::string>()))
                setting = "en-US";

            bool changed = false;
            if (ImGui::BeginCombo(name.data(), languages.at(setting.get<std::string>()).c_str())) {
                for (auto &[languageCode, languageName] : languages) {
                    if (ImGui::Selectable(languageName.c_str(), setting == languageCode)) {
                        setting = languageCode;
                        changed = true;
                    }
                }

                ImGui::EndCombo();
            }

            return changed;
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

        #if defined (OS_LINUX)
            constexpr static auto MultiWindowSupportEnabledDefault = 0;
        #else
            constexpr static auto MultiWindowSupportEnabledDefault = 1;
        #endif

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.multi_windows", MultiWindowSupportEnabledDefault, [](auto name, nlohmann::json &setting) {
            static bool enabled = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &enabled)) {
                setting = static_cast<int>(enabled);
                return true;
            }

            return false;
        }, true);

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

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.restore_window_pos", 0, [](auto name, nlohmann::json &setting) {
            static bool restoreWindowPos = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &restoreWindowPos)) {
                setting = static_cast<int>(restoreWindowPos);
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
        loadThemeSettings();
        loadFoldersSettings();
    }

}
