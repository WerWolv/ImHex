#include <hex/plugin.hpp>

namespace hex::plugin::builtin {

    void registerSettings() {

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.color", 0, [](auto name, nlohmann::json &setting) {
            static int selection = setting.is_number() ? static_cast<int>(setting) : 0;

            const char* themes[] = {
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

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.language", "en-US", [](auto name, nlohmann::json &setting) {
            auto &languages = LangEntry::getSupportedLanguages();

            static int selection = [&]() -> int {
                u16 index = 0;
                for (auto &[languageCode, languageName] : languages){
                    if (languageCode == setting)
                        return index;
                    index++;
                }

                return 0;
            }();

            static auto languageNames = [&]() {
                std::vector<const char*> result;
                for (auto &[languageCode, languageName] : languages)
                    result.push_back(languageName.c_str());

                return result;
            }();


            if (ImGui::Combo(name.data(), &selection, languageNames.data(), languageNames.size())) {

                u16 index = 0;
                for (auto &[languageCode, languageName] : languages){
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
            static int fps = setting.is_number() ? static_cast<int>(setting) : 60;

            if (ImGui::SliderInt(name.data(), &fps, 15, 60)) {
                setting = fps;
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("hex.builtin.setting.interface", "hex.builtin.setting.interface.highlight_alpha", 0x80, [](auto name, nlohmann::json &setting) {
            static int alpha = setting.is_number() ? static_cast<int>(setting) : 0x80;

            if (ImGui::SliderInt(name.data(), &alpha, 0x00, 0xFF)) {
                setting = alpha;
                return true;
            }

            return false;
        });

    }

}