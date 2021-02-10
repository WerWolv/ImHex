#include <hex/plugin.hpp>

namespace hex::plugin::builtin {

    void registerSettings() {

        ContentRegistry::Settings::add("Interface", "Color theme", 0, [](nlohmann::json &setting) {
            static int selection = setting;
            if (ImGui::Combo("##color_theme", &selection, "Dark\0Light\0Classic\0")) {
                setting = selection;
                return true;
            }

            return false;
        });

        ContentRegistry::Settings::add("Interface", "Language", "en-US", [](nlohmann::json &setting) {
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


            if (ImGui::Combo("##language", &selection, languageNames.data(), languageNames.size())) {

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

    }

}