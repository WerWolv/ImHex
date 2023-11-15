#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>
#include <hex/api/theme_manager.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/utils.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <fonts/codicons_font.h>

#include <nlohmann/json.hpp>

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

                if (ImGui::IconButton(ICON_VS_NEW_FOLDER, ImGui::GetStyleColorVec4(ImGuiCol_Text), ImVec2(30, 30))) {
                    fs::openFileBrowser(fs::DialogMode::Folder, {}, [&](const std::fs::path &path) {
                        if (std::find(this->m_paths.begin(), this->m_paths.end(), path) == this->m_paths.end()) {
                            this->m_paths.emplace_back(path);
                            ImHexApi::System::setAdditionalFolderPaths(this->m_paths);

                            result = true;
                        }
                    });
                }
                ImGui::InfoTooltip("hex.builtin.setting.folders.add_folder"_lang);

                if (ImGui::IconButton(ICON_VS_REMOVE_CLOSE, ImGui::GetStyleColorVec4(ImGuiCol_Text), ImVec2(30, 30))) {
                    if (!this->m_paths.empty()) {
                        this->m_paths.erase(std::next(this->m_paths.begin(), this->m_itemIndex));
                        ImHexApi::System::setAdditionalFolderPaths(this->m_paths);

                        result = true;
                    }
                }
                ImGui::InfoTooltip("hex.builtin.setting.folders.remove_folder"_lang);

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

    }

    void registerSettings() {

        /* General */

        namespace Widgets = ContentRegistry::Settings::Widgets;

        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "", "hex.builtin.setting.general.show_tips", true);
        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.general", "", "hex.builtin.setting.general.save_recent_providers", true);
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

        ContentRegistry::Settings::add<Widgets::DropDown>("hex.builtin.setting.interface", "", "hex.builtin.setting.interface.color",
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

        ContentRegistry::Settings::add<ScalingWidget>("hex.builtin.setting.interface", "", "hex.builtin.setting.interface.scaling").requiresRestart();

        std::vector<std::string> languageNames;
        std::vector<nlohmann::json> languageCodes;

        for (auto &[languageCode, languageName] : LangEntry::getSupportedLanguages()) {
            languageNames.emplace_back(languageName);
            languageCodes.emplace_back(languageCode);
        }

        ContentRegistry::Settings::add<Widgets::DropDown>("hex.builtin.setting.interface", "", "hex.builtin.setting.interface.language", languageNames, languageCodes, "en-US");

        ContentRegistry::Settings::add<Widgets::TextBox>("hex.builtin.setting.interface", "", "hex.builtin.setting.interface.wiki_explain_language", "en");
        ContentRegistry::Settings::add<FPSWidget>("hex.builtin.setting.interface", "", "hex.builtin.setting.interface.fps");

        #if defined (OS_LINUX)
            constexpr static auto MultiWindowSupportEnabledDefault = 0;
        #else
            constexpr static auto MultiWindowSupportEnabledDefault = 1;
        #endif

        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "", "hex.builtin.setting.interface.multi_windows", MultiWindowSupportEnabledDefault).requiresRestart();
        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.interface", "", "hex.builtin.setting.interface.restore_window_pos", false);

        ContentRegistry::Settings::add<Widgets::ColorPicker>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.highlight_color", ImColor(0x80, 0x80, 0xC0, 0x60));
        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.sync_scrolling", false);
        ContentRegistry::Settings::add<Widgets::SliderInteger>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.byte_padding", 0, 0, 50);
        ContentRegistry::Settings::add<Widgets::SliderInteger>("hex.builtin.setting.hex_editor", "", "hex.builtin.setting.hex_editor.char_padding", 0, 0, 50);


        /* Fonts */

        ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.font", "hex.builtin.setting.font.glyphs", "hex.builtin.setting.font.load_all_unicode_chars", false);
        auto fontPathSetting = ContentRegistry::Settings::add<Widgets::FilePicker>("hex.builtin.setting.font", "hex.builtin.setting.font.custom_font", "hex.builtin.setting.font.font_path").requiresRestart();
        ContentRegistry::Settings::add<Widgets::SliderInteger>("hex.builtin.setting.font", "hex.builtin.setting.font.custom_font", "hex.builtin.setting.font.font_size", 13, 0, 100)
                .requiresRestart()
                .setEnabledCallback([fontPathSetting]{
                    auto &filePicker = static_cast<Widgets::FilePicker &>(fontPathSetting.getWidget());

                    return !filePicker.getPath().empty();
                });


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
        EventManager::subscribe<EventImHexStartupFinished>([]{
            for (const auto &[name, experiment] : ContentRegistry::Experiments::impl::getExperiments()) {
                ContentRegistry::Settings::add<Widgets::Checkbox>("hex.builtin.setting.experiments", "", experiment.unlocalizedName, false)
                        .setTooltip(LangEntry(experiment.unlocalizedDescription))
                        .setChangedCallback([name](Widgets::Widget &widget) {
                            auto checkBox = static_cast<Widgets::Checkbox *>(&widget);

                            ContentRegistry::Experiments::enableExperiement(name, checkBox->isChecked());
                        });
            }
        });

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
        loadThemeSettings();
        loadFolderSettings();
    }

}
