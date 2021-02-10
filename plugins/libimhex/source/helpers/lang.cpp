#include "hex/helpers/lang.hpp"

#include "hex/helpers/shared_data.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace hex {

    LangEntry::LangEntry(const char *unlocalizedString) : m_unlocalizedString(unlocalizedString) { }

    LangEntry::operator std::string() const {
        return std::string(get());
    }

    LangEntry::operator std::string_view() const {
        return get();
    }

    LangEntry::operator const char*() const {
        return get().data();
    }

    std::string_view LangEntry::get() const {
        auto &lang = SharedData::loadedLanguage;
        if (lang.find(this->m_unlocalizedString) != lang.end())
            return lang[this->m_unlocalizedString];
        else
            return this->m_unlocalizedString;
    }

    void LangEntry::loadLanguage(std::string_view language) {
        SharedData::loadedLanguage.clear();

        bool isDefaultLanguage = language == "en-US";

        try {
            std::ifstream languageFile("lang/" + std::string(language) + ".json");
            nlohmann::json languageJson;

            if (!languageFile.is_open() && !isDefaultLanguage)
                languageFile.open("lang/en-US.json");

            languageFile >> languageJson;

            for (auto &[unlocalizedString, localizedString] : languageJson["lang"].items())
                SharedData::loadedLanguage.insert({ unlocalizedString, localizedString });

            if (!isDefaultLanguage) {
                languageFile.open("lang/en-US.json");
                if (!languageFile.good())
                    return;

                languageFile >> languageJson;

                for (auto &[unlocalizedString, localizedString] : languageJson["lang"].items())
                    SharedData::loadedLanguage.insert({ unlocalizedString, localizedString });
            }
        } catch (std::exception &e) {
            printf("Language load error: %s\n", e.what());

            if (!isDefaultLanguage)
                loadLanguage("en-US");
        }

    }

    const std::map<std::string, std::string>& LangEntry::getSupportedLanguages() {
        static std::map<std::string, std::string> languages;

        if (languages.empty()) {
            for (auto &entry : std::filesystem::directory_iterator("lang")) {
                try {
                    std::ifstream file(entry.path());
                    nlohmann::json json;
                    file >> json;

                    languages.insert({ json["name"].get<std::string>(), entry.path().stem().string() });
                } catch (std::exception &e) {}
            }
        }

        return languages;
    }

}