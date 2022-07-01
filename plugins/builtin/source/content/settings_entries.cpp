#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>

#include <hex/api/localization.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <fonts/codicons_font.h>
#include <hex/helpers/net.hpp>
#include <hex/helpers/utils.hpp>

#include <nlohmann/json.hpp>

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

        /* Interface */

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.color", 0, [](auto name, nlohmann::json &setting) {
            static int selection = static_cast<int>(setting);

            const char *themes[] = {
                "hex.builtin.setting.interface.color.system"_lang,
                "hex.builtin.setting.interface.color.dark"_lang,
                "hex.builtin.setting.interface.color.light"_lang,
                "hex.builtin.setting.interface.color.classic"_lang
            };

            if (ImGui::Combo(name.data(), &selection, themes, IM_ARRAYSIZE(themes))) {
                setting = selection;
                return true;
            }

            return false;
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

                    ImHexApi::Common::restartImHex();

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
                    if (languageCode == setting)
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

            auto format = fps > 200 ? "hex.builtin.setting.interface.fps.unlocked"_lang : "%d FPS";

            if (ImGui::SliderInt(name.data(), &fps, 15, 201, format, ImGuiSliderFlags_AlwaysClamp)) {
                setting = fps;
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
                        result = true;
                    }
                }

                ImGui::EndCombo();
            }

            return result;
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
                    return fs::openFileBrowser(fs::DialogMode::Open, { {"TTF Font", "ttf"} },
                        [&](const std::fs::path &path) {
                            fontPath = path.string();
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

                return false;
            },
            true);


        /* Folders */

        static const std::string dirsSetting { "hex.builtin.setting.folders" };

        ContentRegistry::Settings::addCategoryDescription(dirsSetting, "hex.builtin.setting.folders.description");

        ContentRegistry::Settings::add(dirsSetting, dirsSetting, std::vector<std::string> {}, [](auto name, nlohmann::json &setting) {
            hex::unused(name);

            static std::vector<std::string> folders = setting;
            static size_t currentItemIndex             = 0;

            bool result = false;

            if (!ImGui::BeginListBox("", ImVec2(-38, -FLT_MIN))) {
                return false;
            } else {
                for (size_t n = 0; n < folders.size(); n++) {
                    const bool isSelected = (currentItemIndex == n);
                    if (ImGui::Selectable(folders.at(n).c_str(), isSelected)) { currentItemIndex = n; }
                    if (isSelected) { ImGui::SetItemDefaultFocus(); }
                }
                ImGui::EndListBox();
            }
            ImGui::SameLine();
            ImGui::BeginGroup();

            if (ImGui::IconButton(ICON_VS_NEW_FOLDER, ImGui::GetCustomColorVec4(ImGuiCustomCol_DescButton), ImVec2(30, 30))) {
                fs::openFileBrowser(fs::DialogMode::Folder, {}, [&](const std::fs::path &path) {
                    auto pathStr = path.string();

                    if (std::find(folders.begin(), folders.end(), pathStr) == folders.end()) {
                        folders.emplace_back(pathStr);
                        ContentRegistry::Settings::write(dirsSetting, dirsSetting, folders);
                        result = true;
                    }
                });
            }
            ImGui::InfoTooltip("hex.builtin.setting.folders.add_folder"_lang);

            if (ImGui::IconButton(ICON_VS_REMOVE_CLOSE, ImGui::GetCustomColorVec4(ImGuiCustomCol_DescButton), ImVec2(30, 30))) {
                if (!folders.empty()) {
                    folders.erase(std::next(folders.begin(), currentItemIndex));
                    ContentRegistry::Settings::write(dirsSetting, dirsSetting, folders);
                    result = true;
                }
            }
            ImGui::InfoTooltip("hex.builtin.setting.folders.remove_folder"_lang);

            ImGui::EndGroup();

            return result;
        });

        /* Proxy */

        static const std::string proxySetting { "hex.builtin.setting.proxy" };

        // init hex::Net proxy url
        hex::Net::setProxy(ContentRegistry::Settings::read(proxySetting, "hex.builtin.setting.proxy.url", ""));

        ContentRegistry::Settings::addCategoryDescription(proxySetting, "hex.builtin.setting.proxy.description");

        ContentRegistry::Settings::add(
            proxySetting, "hex.builtin.setting.proxy.url", "", [](auto name, nlohmann::json &setting) {
                static std::string proxyUrl = static_cast<std::string>(setting);
                static bool enableProxy     = !proxyUrl.empty();

                bool result = false;

                if (ImGui::Checkbox("hex.builtin.setting.proxy.enable"_lang, &enableProxy)) {
                    setting = enableProxy ? proxyUrl : "";
                    hex::Net::setProxy(enableProxy ? proxyUrl : "");
                    result = true;
                }

                ImGui::BeginDisabled(!enableProxy);
                if (ImGui::InputText("##proxy_url", proxyUrl)) {
                    setting = proxyUrl;
                    hex::Net::setProxy(proxyUrl);
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

}
