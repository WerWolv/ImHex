#include <hex/api/content_registry.hpp>
#include <hex/api/localization_manager.hpp>

#include <hex/helpers/auto_reset.hpp>
#include <hex/helpers/logger.hpp>

#include <nlohmann/json.hpp>

namespace hex {

    namespace LocalizationManager {

        namespace {

            AutoReset<std::map<std::string, LanguageDefinition>> s_languageDefinitions;
            AutoReset<std::unordered_map<std::size_t, std::string>> s_localizations;
            AutoReset<std::string> s_selectedLanguageId;

        }

        void addLanguages(const std::string_view &languageList, std::function<std::string_view(const std::filesystem::path &path)> callback) {
            const auto json = nlohmann::json::parse(languageList);

            for (const auto &item : json) {
                if (!item.contains("code") || !item.contains("path")) {
                    log::error("Invalid language definition: {}", item.dump(4));
                    continue;
                }

                auto &definition = (*s_languageDefinitions)[item["code"].get<std::string>()];

                if (definition.name.empty() && item.contains("name")) {
                    definition.name = item["name"].get<std::string>();
                }

                if (definition.nativeName.empty() && item.contains("native_name")) {
                    definition.nativeName = item["native_name"].get<std::string>();
                }

                if (definition.fallbackLanguageId.empty() && item.contains("fallback")) {
                    definition.fallbackLanguageId = item["fallback"].get<std::string>();
                }

                definition.languageFilePaths.emplace_back(PathEntry{ item["path"].get<std::string>(), callback });
            }
        }

        static void populateLocalization(const std::string &languageId) {
            if (languageId.empty())
                return;

            log::debug("Populating localization for language: {}", languageId);

            const auto &definition = (*s_languageDefinitions)[languageId];
            for (const auto &path : definition.languageFilePaths) {
                const auto translation = path.callback(path.path);
                const auto json = nlohmann::json::parse(translation);

                for (const auto &entry : json.items()) {
                    auto value = entry.value().get<std::string>();
                    if (value.empty())
                        continue;

                    s_localizations->try_emplace(LangConst::hash(entry.key()), std::move(value));
                }
            }

            populateLocalization(definition.fallbackLanguageId);
        }

        void setLanguage(const std::string &languageId) {
            s_localizations->clear();
            s_selectedLanguageId = languageId;

            populateLocalization(languageId);
        }

        [[nodiscard]] const std::string& getSelectedLanguageId() {
            return *s_selectedLanguageId;
        }

        const std::map<std::string, LanguageDefinition>& getLanguageDefinitions() {
            return *s_languageDefinitions;
        }

    }

    Lang::Lang(const char *unlocalizedString) : m_entryHash(LangConst::hash(unlocalizedString)), m_unlocalizedString(unlocalizedString) { }
    Lang::Lang(const std::string &unlocalizedString) : m_entryHash(LangConst::hash(unlocalizedString)), m_unlocalizedString(unlocalizedString) { }
    Lang::Lang(const LangConst &localizedString) : m_entryHash(localizedString.m_entryHash), m_unlocalizedString(localizedString.m_unlocalizedString) { }
    Lang::Lang(const UnlocalizedString &unlocalizedString) : m_entryHash(LangConst::hash(unlocalizedString.get())), m_unlocalizedString(unlocalizedString.get()) { }
    Lang::Lang(std::string_view unlocalizedString) : m_entryHash(LangConst::hash(unlocalizedString)), m_unlocalizedString(unlocalizedString) { }

    Lang::operator std::string() const {
        return get();
    }

    Lang::operator std::string_view() const {
        return get();
    }

    Lang::operator const char *() const {
        return get();
    }

    const char *Lang::get() const {
        const auto &lang = *LocalizationManager::s_localizations;

        const auto it = lang.find(m_entryHash);
        if (it == lang.end()) {
            return m_unlocalizedString.c_str();
        } else {
            return it->second.c_str();
        }
    }

    LangConst::operator std::string() const {
        return get();
    }

    LangConst::operator std::string_view() const {
        return get();
    }

    LangConst::operator const char *() const {
        return get();
    }

    const char *LangConst::get() const {
        const auto &lang = *LocalizationManager::s_localizations;

        const auto it = lang.find(m_entryHash);
        if (it == lang.end()) {
            return m_unlocalizedString;
        } else {
            return it->second.c_str();
        }
    }

}