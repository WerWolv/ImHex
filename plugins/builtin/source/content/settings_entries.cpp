#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/theme_manager.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/event_manager.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/utils.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <fonts/codicons_font.h>

#include <nlohmann/json.hpp>

#include <utility>
#include <hex/api/layout_manager.hpp>
#include <wolv/utils/string.hpp>

namespace hex::plugin::builtin {

    namespace {

        /*
            Values of this setting:
            0 - do not check for updates on startup
            1 - check for updates on startup
            2 - default value - ask the user if he wants to check for updates. This value should only be encountered on the first startup.
        */
        class ServerContactWidget : public ContentRegistry::Settings::Widgets::Widget {
        public:
            bool draw(const std::string &name) override {
                bool enabled = this->m_value == 1;

                if (ImGui::Checkbox(name.data(), &enabled)) {
                    this->m_value = enabled ? 1 : 0;
                    return true;
                }

                return false;
            }

            void load(const nlohmann::json &data) override {
                if (data.is_number())
                    this->m_value = data.get<int>();
            }

            nlohmann::json store() override {
                return this->m_value;
            }

        private:
            u32 m_value = 2;
        };

        class FPSWidget : public ContentRegistry::Settings::Widgets::Widget {
        public:
            bool draw(const std::string &name) override {
                auto format = [this] -> std::string {
                    if (this->m_value > 200)
                        return "hex.builtin.setting.interface.fps.unlocked"_lang;
                    else if (this->m_value < 15)
                        return "hex.builtin.setting.interface.fps.native"_lang;
                    else
                        return "%d FPS";
                }();

                if (ImGui::SliderInt(name.data(), &this->m_value, 14, 201, format.c_str(), ImGuiSliderFlags_AlwaysClamp)) {
                    return true;
                }

                return false;
            }

            void load(const nlohmann::json &data) override {
                if (data.is_number())
                    this->m_value = data.get<int>();
            }

            nlohmann::json store() override {
                return this->m_value;
            }

        private:
            int m_value = 60;
        };

        class UserFolderWidget : public ContentRegistry::Settings::Widgets::Widget {
        public:
            bool draw(const std::string &) override {
                bool result = false;

                if (!ImGui::BeginListBox("", ImVec2(-40_scaled, 280_scaled))) {
                    return false;
                } else {
                    for (size_t n = 0; n < this->m_paths.size(); n++) {
                        const bool isSelected = (this->m_itemIndex == n);
                        if (ImGui::Selectable(wolv::util::toUTF8String(this->m_paths[n]).c_str(), isSelected)) {
                            this->m_itemIndex = n;
                        }

                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndListBox();
                }
                ImGui::SameLine();
                ImGui::BeginGroup();

                if (ImGuiExt::IconButton(ICON_VS_NEW_FOLDER, ImGui::GetStyleColorVec4(ImGuiCol_Text), ImVec2(30, 30))) {
                    fs::openFileBrowser(fs::DialogMode::Folder, {}, [&](const std::fs::path &path) {
                        if (std::find(this->m_paths.begin(), this->m_paths.end(), path) == this->m_paths.end()) {
                            this->m_paths.emplace_back(path);
                            ImHexApi::System::setAdditionalFolderPaths(this->m_paths);

                            result = true;
                        }
                    });
                }
                ImGuiExt::InfoTooltip("hex.builtin.setting.folders.add_folder"_lang);

                if (ImGuiExt::IconButton(ICON_VS_REMOVE_CLOSE, ImGui::GetStyleColorVec4(ImGuiCol_Text), ImVec2(30, 30))) {
                    if (!this->m_paths.empty()) {
                        this->m_paths.erase(std::next(this->m_paths.begin(), this->m_itemIndex));
                        ImHexApi::System::setAdditionalFolderPaths(this->m_paths);

                        result = true;
                    }
                }
                ImGuiExt::InfoTooltip("hex.builtin.setting.folders.remove_folder"_lang);

                ImGui::EndGroup();

                return result;
            }

            void load(const nlohmann::json &data) override {
                if (data.is_array()) {
                    std::vector<std::string> pathStrings = data;

                    for (const auto &pathString : pathStrings) {
                        this->m_paths.emplace_back(pathString);
                    }
                }
            }

            nlohmann::json store() override {
                std::vector<std::string> pathStrings;

                for (const auto &path : this->m_paths) {
                    pathStrings.push_back(wolv::util::toUTF8String(path));
                }

                return pathStrings;
            }

        private:
            u32 m_itemIndex = 0;
            std::vector<std::fs::path> m_paths;
        };

        class ScalingWidget : public ContentRegistry::Settings::Widgets::Widget {
        public:
            bool draw(const std::string &name) override {
                auto format = [this] -> std::string {
                    if (this->m_value == 0)
                        return "hex.builtin.setting.interface.scaling.native"_lang;
                    else
                        return "x%.1f";
                }();

                if (ImGui::SliderFloat(name.data(), &this->m_value, 0, 10, format.c_str(), ImGuiSliderFlags_AlwaysClamp)) {
                    return true;
                }

                return false;
            }

            void load(const nlohmann::json &data) override {
                if (data.is_number())
                    this->m_value = data.get<float>();
            }

            nlohmann::json store() override {
                return this->m_value;
            }

        private:
            float m_value = 0;
        };

        class AutoBackupWidget : public ContentRegistry::Settings::Widgets::Widget {
        public:
            bool draw(const std::string &name) override {
                auto format = [this] -> std::string {
                    auto value = this->m_value * 30;
                    if (value == 0)
                        return "hex.builtin.common.off"_lang;
                    else if (value < 60)
                        return hex::format("hex.builtin.setting.general.auto_backup_time.format.simple"_lang, value);
                    else
                        return hex::format("hex.builtin.setting.general.auto_backup_time.format.extended"_lang, value / 60, value % 60);
                }();

                if (ImGui::SliderInt(name.data(), &this->m_value, 0, (30 * 60) / 30, format.c_str(), ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoInput)) {
                    return true;
                }

                return false;
            }

            void load(const nlohmann::json &data) override {
                if (data.is_number())
                    this->m_value = data.get<int>();
            }

            nlohmann::json store() override {
                return this->m_value;
            }

        private:
            int m_value = 0;
        };

        class KeybindingWidget : public ContentRegistry::Settings::Widgets::Widget {
        public:
            KeybindingWidget(View *view, const Shortcut &shortcut) : m_view(view), m_shortcut(shortcut), m_drawShortcut(shortcut), m_defaultShortcut(shortcut) {}

            bool draw(const std::string &name) override {
                std::string label;

                if (!this->m_editing)
                    label = this->m_drawShortcut.toString();
                else
                    label = "...";

                if (label.empty())
                    label = "???";


                if (this->m_hasDuplicate)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_LoggerError));

                ImGui::PushID(this);
                if (ImGui::Button(label.c_str(), ImVec2(250_scaled, 0))) {
                    this->m_editing = !this->m_editing;

                    if (this->m_editing)
                        ShortcutManager::pauseShortcuts();
                    else
                        ShortcutManager::resumeShortcuts();
                }

                ImGui::SameLine();

                if (this->m_hasDuplicate)
                    ImGui::PopStyleColor();

                bool settingChanged = false;

                ImGui::BeginDisabled(this->m_drawShortcut == this->m_defaultShortcut);
                if (ImGuiExt::IconButton(ICON_VS_X, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    this->m_hasDuplicate = !ShortcutManager::updateShortcut(this->m_shortcut, this->m_defaultShortcut, this->m_view);

                    this->m_drawShortcut = this->m_defaultShortcut;
                    if (!this->m_hasDuplicate) {
                        this->m_shortcut = this->m_defaultShortcut;
                        settingChanged = true;
                    }

                }
                ImGui::EndDisabled();

                if (!ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    this->m_editing = false;
                    ShortcutManager::resumeShortcuts();
                }

                ImGui::SameLine();

                ImGuiExt::TextFormatted("{}", name);

                ImGui::PopID();

                if (this->m_editing) {
                    if (this->detectShortcut()) {
                        this->m_editing = false;
                        ShortcutManager::resumeShortcuts();

                            settingChanged = true;
                        if (!this->m_hasDuplicate) {
                        }
                    }
                }

                return settingChanged;
            }

            void load(const nlohmann::json &data) override {
                std::set<Key> keys;

                for (const auto &key : data.get<std::vector<u32>>())
                    keys.insert(Key(Keys(key)));

                if (keys.empty())
                    return;

                auto newShortcut = Shortcut(keys);
                this->m_hasDuplicate = !ShortcutManager::updateShortcut(this->m_shortcut, newShortcut, this->m_view);
                this->m_shortcut = std::move(newShortcut);
                this->m_drawShortcut = this->m_shortcut;
            }

            nlohmann::json store() override {
                std::vector<u32> keys;

                for (const auto &key : this->m_shortcut.getKeys()) {
                    if (key != CurrentView)
                        keys.push_back(key.getKeyCode());
                }

                return keys;
            }

        private:
            bool detectShortcut() {
                if (const auto &shortcut = ShortcutManager::getPreviousShortcut(); shortcut.has_value()) {
                    auto keys = this->m_shortcut.getKeys();
                    std::erase_if(keys, [](Key key) {
                        return key != AllowWhileTyping && key != CurrentView;
                    });

                    for (const auto &key : shortcut->getKeys()) {
                        keys.insert(key);
                    }

                    auto newShortcut = Shortcut(std::move(keys));
                    this->m_hasDuplicate = !ShortcutManager::updateShortcut(this->m_shortcut, newShortcut, this->m_view);
                    this->m_drawShortcut = std::move(newShortcut);

                    if (!this->m_hasDuplicate) {
                        this->m_shortcut = this->m_drawShortcut;
                        log::info("Changed shortcut to {}", shortcut->toString());
                    } else {
                        log::warn("Changing shortcut failed as it overlapped with another one", shortcut->toString());
                    }

                    return true;
                }

                return false;
            }

        private:
            View *m_view = nullptr;
            Shortcut m_shortcut, m_drawShortcut, m_defaultShortcut;
            bool m_editing = false;
            bool m_hasDuplicate = false;
        };

    }

    void registerSettings() {

        /* General */

        namespace Widgets = ContentRegistry::Settings::Widgets;

        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "", "hex.builtin.setting.general.show_tips", false);
        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "", "hex.builtin.setting.general.save_recent_providers", true);
        ContentRegistry::Settings::add<AutoBackupWidget>("hex.builtin.setting.general", "", "hex.builtin.setting.general.auto_backup_time");
        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "hex.builtin.setting.general.patterns", "hex.builtin.setting.general.auto_load_patterns", true);
        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "hex.builtin.setting.general.patterns", "hex.builtin.setting.general.sync_pattern_source", false);
        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "hex.builtin.setting.general.network", "hex.builtin.setting.general.network_interface", false);
        ContentRegistry::Settings::add<ServerContactWidget>("hex.builtin.setting.general", "hex.builtin.setting.general.network", "hex.builtin.setting.general.server_contact");
        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "hex.builtin.setting.general.network", "hex.builtin.setting.general.upload_crash_logs", true);

        /* Interface */

        auto themeNames = ThemeManager::getThemeNames();
        std::vector<nlohmann::json> themeJsons = { };
        for (const auto &themeName : themeNames)
            themeJsons.emplace_back(themeName);

        themeNames.emplace(themeNames.begin(), ThemeManager::NativeTheme);
        themeJsons.emplace(themeJsons.begin(), ThemeManager::NativeTheme);

        ContentRegistry::Settings::add<Widgets::DropDown>("hex.builtin.setting.interface", "hex.builtin.setting.interface.style", "hex.builtin.setting.interface.color",
                                                          themeNames,
                                                          themeJsons,
                                                          "Dark").setChangedCallback([](auto &widget) {
                                                              auto dropDown = static_cast<Widgets::DropDown *>(&widget);

                                                              if (dropDown->getValue() == ThemeManager::NativeTheme)
                                                                  ImHexApi::System::enableSystemThemeDetection(true);
                                                              else {
                                                                  ImHexApi::System::enableSystemThemeDetection(false);
                                                                  ThemeManager::changeTheme(dropDown->getValue());
                                                              }
                                                          });

        ContentRegistry::Settings::add<ScalingWidget>("hex.builtin.setting.interface", "hex.builtin.setting.interface.style", "hex.builtin.setting.interface.scaling_factor").requiresRestart();

        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.style", "hex.builtin.setting.interface.pattern_data_row_bg", false);

        std::vector<std::string> languageNames;
        std::vector<nlohmann::json> languageCodes;

        for (auto &[languageCode, languageName] : LocalizationManager::getSupportedLanguages()) {
            languageNames.emplace_back(languageName);
            languageCodes.emplace_back(languageCode);
        }

        ContentRegistry::Settings::add<Widgets::DropDown>("hex.builtin.setting.interface", "hex.builtin.setting.interface.language", "hex.builtin.setting.interface.language", languageNames, languageCodes, "en-US");

        ContentRegistry::Settings::add<Widgets::TextBox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.language", "hex.builtin.setting.interface.wiki_explain_language", "en");
        ContentRegistry::Settings::add<FPSWidget>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window", "hex.builtin.setting.interface.fps");

        #if defined (OS_LINUX)
            constexpr static auto MultiWindowSupportEnabledDefault = 0;
        #else
            constexpr static auto MultiWindowSupportEnabledDefault = 1;
        #endif

        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window", "hex.builtin.setting.interface.multi_windows", MultiWindowSupportEnabledDefault).requiresRestart();
        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window", "hex.builtin.setting.interface.restore_window_pos", false);

        ContentRegistry::Settings::add<Widgets::ColorPicker>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.highlight_color", ImColor(0x80, 0x80, 0xC0, 0x60));
        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.sync_scrolling", false);
        ContentRegistry::Settings::add<Widgets::SliderInteger>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.byte_padding", 0, 0, 50);
        ContentRegistry::Settings::add<Widgets::SliderInteger>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.char_padding", 0, 0, 50);


        /* Fonts */

        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.font", "hex.builtin.setting.font.glyphs", "hex.builtin.setting.font.load_all_unicode_chars", false)
            .requiresRestart();

        auto customFontEnabledSetting = ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.font", "hex.builtin.setting.font.custom_font", "hex.builtin.setting.font.custom_font_enable", false).requiresRestart();

        const auto customFontsEnabled = [customFontEnabledSetting] {
            auto &customFontsEnabled = static_cast<Widgets::Checkbox &>(customFontEnabledSetting.getWidget());

            return customFontsEnabled.isChecked();
        };

        auto customFontPathSetting = ContentRegistry::Settings::add<Widgets::FilePicker>("hex.builtin.setting.font", "hex.builtin.setting.font.custom_font", "hex.builtin.setting.font.font_path")
                .requiresRestart()
                .setEnabledCallback(customFontsEnabled);

        const auto customFontSettingsEnabled = [customFontEnabledSetting, customFontPathSetting] {
            auto &customFontsEnabled = static_cast<Widgets::Checkbox &>(customFontEnabledSetting.getWidget());
            auto &fontPath = static_cast<Widgets::FilePicker &>(customFontPathSetting.getWidget());

            return customFontsEnabled.isChecked() && !fontPath.getPath().empty();
        };

        ContentRegistry::Settings::add<Widgets::Label>("hex.builtin.setting.font", "hex.builtin.setting.font.custom_font", "hex.builtin.setting.font.custom_font_info")
                .setEnabledCallback(customFontsEnabled);


        ContentRegistry::Settings::add<Widgets::SliderInteger>("hex.builtin.setting.font", "hex.builtin.setting.font.custom_font", "hex.builtin.setting.font.font_size", 13, 0, 100)
                .requiresRestart()
                .setEnabledCallback(customFontSettingsEnabled);
        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.font", "hex.builtin.setting.font.custom_font", "hex.builtin.setting.font.font_bold", false)
                .requiresRestart()
                .setEnabledCallback(customFontSettingsEnabled);
        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.font", "hex.builtin.setting.font.custom_font", "hex.builtin.setting.font.font_italic", false)
                .requiresRestart()
                .setEnabledCallback(customFontSettingsEnabled);
        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.font", "hex.builtin.setting.font.custom_font", "hex.builtin.setting.font.font_antialias", false)
                .requiresRestart()
                .setEnabledCallback(customFontSettingsEnabled);



        /* Folders */

        ContentRegistry::Settings::setCategoryDescription("hex.builtin.setting.folders", "hex.builtin.setting.folders.description");
        ContentRegistry::Settings::add<UserFolderWidget>("hex.builtin.setting.folders", "", "hex.builtin.setting.folders.description");

        /* Proxy */

        HttpRequest::setProxyUrl(ContentRegistry::Settings::read("hex.builtin.setting.proxy", "hex.builtin.setting.proxy.url", "").get<std::string>());

        ContentRegistry::Settings::setCategoryDescription("hex.builtin.setting.proxy", "hex.builtin.setting.proxy.description");

        auto proxyEnabledSetting = ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.proxy", "", "hex.builtin.setting.proxy.enable", false).setChangedCallback([](Widgets::Widget &widget) {
            auto checkBox = static_cast<Widgets::Checkbox *>(&widget);

            HttpRequest::setProxyState(checkBox->isChecked());
        });

        ContentRegistry::Settings::add<Widgets::TextBox>("hex.builtin.setting.proxy", "", "hex.builtin.setting.proxy.url", "")
        .setEnabledCallback([proxyEnabledSetting] {
            auto &checkBox = static_cast<Widgets::Checkbox &>(proxyEnabledSetting.getWidget());

            return checkBox.isChecked();
        })
        .setChangedCallback([](Widgets::Widget &widget) {
            auto textBox = static_cast<Widgets::TextBox *>(&widget);

            HttpRequest::setProxyUrl(textBox->getValue());
        });


        /* Experiments */
        ContentRegistry::Settings::setCategoryDescription("hex.builtin.setting.experiments", "hex.builtin.setting.experiments.description");
        EventImHexStartupFinished::subscribe([]{
            for (const auto &[name, experiment] : ContentRegistry::Experiments::impl::getExperiments()) {
                ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.experiments", "", experiment.unlocalizedName, false)
                        .setTooltip(Lang(experiment.unlocalizedDescription))
                        .setChangedCallback([name](Widgets::Widget &widget) {
                            auto checkBox = static_cast<Widgets::Checkbox *>(&widget);

                            ContentRegistry::Experiments::enableExperiement(name, checkBox->isChecked());
                        });
            }
        });

        /* Shorcuts */
        EventImHexStartupFinished::subscribe([]{
            for (const auto &shortcutEntry : ShortcutManager::getGlobalShortcuts()) {
                ContentRegistry::Settings::add<KeybindingWidget>("hex.builtin.setting.shortcuts", "hex.builtin.setting.shortcuts.global", shortcutEntry.unlocalizedName, nullptr, shortcutEntry.shortcut);
            }

            for (auto &[viewName, view] : ContentRegistry::Views::impl::getEntries()) {
                for (const auto &shortcutEntry : ShortcutManager::getViewShortcuts(view.get())) {
                    ContentRegistry::Settings::add<KeybindingWidget>("hex.builtin.setting.shortcuts", viewName, shortcutEntry.unlocalizedName, view.get(), shortcutEntry.shortcut);
                }
            }
       });

    }

    static void loadLayoutSettings() {
        const bool locked = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.layout_locked", false);
        LayoutManager::lockLayout(locked);
    }

    static void loadThemeSettings() {
        auto theme = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.color", ThemeManager::NativeTheme).get<std::string>();

        if (theme == ThemeManager::NativeTheme)
            ImHexApi::System::enableSystemThemeDetection(true);
        else {
            ImHexApi::System::enableSystemThemeDetection(false);
            ThemeManager::changeTheme(theme);
        }
    }

    static void loadFolderSettings() {
        auto folderPathStrings = ContentRegistry::Settings::read("hex.builtin.setting.folders", "hex.builtin.setting.folders", std::vector<std::string> { });

        std::vector<std::fs::path> paths;
        for (const auto &pathString : folderPathStrings) {
            paths.emplace_back(pathString);
        }

        ImHexApi::System::setAdditionalFolderPaths(paths);
    }

    void loadSettings() {
        loadLayoutSettings();
        loadThemeSettings();
        loadFolderSettings();
    }

}
