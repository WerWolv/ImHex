#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>

#include <hex/api/localization.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <fonts/codicons_font.h>

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

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.fps", 60, [](auto name, nlohmann::json &setting) {
            static int fps = static_cast<int>(setting);

            auto format = fps > 200 ? "hex.builtin.setting.interface.fps.unlocked"_lang : "%d FPS";

            if (ImGui::SliderInt(name.data(), &fps, 15, 201, format, ImGuiSliderFlags_AlwaysClamp)) {
                setting = fps;
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.highlight_alpha", 0x80, [](auto name, nlohmann::json &setting) {
            static int alpha = static_cast<int>(setting);

            if (ImGui::SliderInt(name.data(), &alpha, 0x00, 0xFF)) {
                setting = alpha;
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.column_count", 16, [](auto name, nlohmann::json &setting) {
            static int columns = static_cast<int>(setting);

            if (ImGui::SliderInt(name.data(), &columns, 1, 32)) {
                setting = columns;
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.hexii", 0, [](auto name, nlohmann::json &setting) {
            static bool hexii = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &hexii)) {
                setting = static_cast<int>(hexii);
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

        ContentRegistry::Settings::add("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.extra_info", 1, [](auto name, nlohmann::json &setting) {
            static bool extraInfos = static_cast<int>(setting);

            if (ImGui::Checkbox(name.data(), &extraInfos)) {
                setting = static_cast<int>(extraInfos);
                return true;
            }

            return false;
        });


        static std::string fontPath;
        ContentRegistry::Settings::add(
            "hex.builtin.setting.font", "hex.builtin.setting.font.font_path", "", [](auto name, nlohmann::json &setting) {
                fontPath = static_cast<std::string>(setting);

                if (ImGui::InputText("##font_path", fontPath.data(), fontPath.capacity(), ImGuiInputTextFlags_CallbackResize, ImGui::UpdateStringSizeCallback, &fontPath)) {
                    setting = fontPath;
                    return true;
                }

                ImGui::SameLine();

                if (ImGui::IconButton(ICON_VS_FOLDER_OPENED, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    return hex::openFileBrowser("hex.builtin.setting.font.font_path", DialogMode::Open, {
                                                                                                            {"TTF Font", "ttf"}
                    },
                        [&](const fs::path &path) {
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

                if (ImGui::SliderInt(name.data(), &fontSize, 0, 100, "%d", ImGuiSliderFlags_NoInput)) {
                    setting = fontSize;
                    return true;
                }

                ImGui::EndDisabled();

                return false;
            },
            true);


        static const std::string dirsSetting { "hex.builtin.setting.folders" };

        ContentRegistry::Settings::addCategoryDescription(dirsSetting, "hex.builtin.setting.folders.description");

        ContentRegistry::Settings::add(dirsSetting, dirsSetting, std::vector<std::string> {}, [](auto name, nlohmann::json &setting) {
            static std::vector<std::string> folders = setting;
            static int currentItemIndex             = 0;

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
                hex::openFileBrowser("Select include folder", hex::DialogMode::Folder, {}, [&](fs::path path) {
                    auto pathStr = path.string();

                    if (std::find(folders.begin(), folders.end(), pathStr) == folders.end()) {
                        folders.emplace_back(pathStr);
                        ContentRegistry::Settings::write(dirsSetting, dirsSetting, folders);
                    }
                });
            }

            if (ImGui::IconButton(ICON_VS_REMOVE_CLOSE, ImGui::GetCustomColorVec4(ImGuiCustomCol_DescButton), ImVec2(30, 30))) {
                if (!folders.empty()) {
                    folders.erase(std::next(folders.begin(), currentItemIndex));
                    ContentRegistry::Settings::write(dirsSetting, dirsSetting, folders);
                }
            }

            ImGui::EndGroup();

            return true;
        });
    }

}
