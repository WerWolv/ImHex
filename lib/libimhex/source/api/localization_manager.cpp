#include <hex/api/localization_manager.hpp>

#include <hex/helpers/auto_reset.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/utils.hpp>

#include <nlohmann/json.hpp>

#include <mutex>
#include <hex/helpers/debugging.hpp>

namespace hex {

    namespace LocalizationManager {

        constexpr static auto FallbackLanguageId = "en-US";

        namespace {

            AutoReset<std::map<LanguageId, LanguageDefinition>> s_languageDefinitions;
            AutoReset<std::unordered_map<std::size_t, std::string>> s_localizations;
            AutoReset<LanguageId> s_selectedLanguageId;

        }

        void addLanguages(const std::string_view &languageList, std::function<std::string_view(const std::string &path)> callback) {
            const auto json = nlohmann::json::parse(languageList);

            for (const auto &item : json) {
                if (!item.contains("code") || !item.contains("path")) {
                    log::error("Invalid language definition: {}", item.dump(4));
                    continue;
                }

                auto &definition = (*s_languageDefinitions)[item["code"].get<std::string>()];

                if (definition.id.empty()) {
                    definition.id = item["code"].get<std::string>();
                }

                if (definition.name.empty() && item.contains("name")) {
                    definition.name = item["name"].get<std::string>();
                }

                if (definition.nativeName.empty() && item.contains("native_name")) {
                    definition.nativeName = item["native_name"].get<std::string>();
                }

                if (definition.fallbackLanguageId.empty() && item.contains("fallback")) {
                    definition.fallbackLanguageId = item["fallback"].get<std::string>();
                }

                if (item.contains("hidden") && item["hidden"].get<bool>()) {
                    definition.hidden = true;
                }

                const auto path = item["path"].get<std::string>();

                definition.languageFilePaths.emplace_back(PathEntry{ path, callback });
            }
        }

        static LanguageId findBestLanguageMatch(LanguageId languageId) {
            if (s_languageDefinitions->contains(languageId))
                return languageId;

            if (const auto pos = languageId.find('_'); pos != std::string::npos) {
                // Turn language Ids like "en_US" into "en-US"
                languageId[pos] = '-';
            }

            if (const auto pos = languageId.find('-'); pos != std::string::npos) {
                // Try to find a match with the language code without region
                languageId = languageId.substr(0, pos);

                for (const auto &definition : *s_languageDefinitions) {
                    if (definition.first.starts_with(languageId) || definition.first.starts_with(toLower(languageId))) {
                        return definition.first;
                    }
                }
            }

            // Fall back to English if no better match was found
            return "en-US";
        }

        static void populateLocalization(LanguageId languageId, std::unordered_map<std::size_t, std::string> &localizations) {
            if (languageId.empty())
                return;

            languageId = findBestLanguageMatch(languageId);

            if (const auto it = s_languageDefinitions->find(languageId); it == s_languageDefinitions->end()) {
                log::error("No language definition found for language: {}", languageId);

                if (languageId != FallbackLanguageId)
                    populateLocalization(FallbackLanguageId, localizations);
            } else {
                const auto &definition = it->second;
                for (const auto &path : definition.languageFilePaths) {
                    try {
                        const auto translation = path.callback(path.path);
                        const auto json = nlohmann::json::parse(translation);

                        for (const auto &entry : json.items()) {
                            auto value = entry.value().get<std::string>();

                            // Skip empty values
                            if (value.empty())
                                continue;

                            // Handle references to files
                            if (value.starts_with("#@")) {
                                try {
                                    value = path.callback(value.substr(2));
                                } catch (std::exception &e) {
                                    log::error("Failed to load localization file reference '{}': {}", entry.key(), e.what());
                                    continue;
                                }
                            }

                            localizations.try_emplace(LangConst::hash(entry.key()), std::move(value));
                        }
                    } catch (std::exception &e) {
                        log::error("Failed to load localization file '{}': {}", path.path, e.what());
                    }
                }

                populateLocalization(definition.fallbackLanguageId, localizations);
            }
        }

        void setLanguage(const LanguageId &languageId) {
            if (languageId == "native") {
                setLanguage(hex::getOSLanguage().value_or(FallbackLanguageId));
                s_selectedLanguageId = languageId;
                return;
            }

            if (*s_selectedLanguageId == languageId)
                return;

            s_localizations->clear();
            s_selectedLanguageId = languageId;

            populateLocalization(languageId, s_localizations);
        }

        [[nodiscard]] const std::string& getSelectedLanguageId() {
            return *s_selectedLanguageId;
        }

        [[nodiscard]] const std::string& get(const LanguageId &languageId, const UnlocalizedString &unlocalizedString) {
            static AutoReset<LanguageId> currentLanguageId;
            static AutoReset<std::unordered_map<std::size_t, std::string>> loadedLocalization;
            static std::mutex mutex;

            std::scoped_lock lock(mutex);
            if (*currentLanguageId != languageId) {
                currentLanguageId = languageId;
                loadedLocalization->clear();
                populateLocalization(languageId, *loadedLocalization);
            }

            return (*loadedLocalization)[LangConst::hash(unlocalizedString.get())];
        }

        const std::map<std::string, LanguageDefinition>& getLanguageDefinitions() {
            return *s_languageDefinitions;
        }

        const LanguageDefinition& getLanguageDefinition(const LanguageId &languageId) {
            const auto bestMatch = findBestLanguageMatch(languageId);
            const auto &result = (*s_languageDefinitions)[bestMatch];

            if (!dbg::debugModeEnabled()) {
                if (result.hidden)
                    return getLanguageDefinition(result.fallbackLanguageId);
            }

            return result;
        }

    }

    static AutoReset<std::map<std::size_t, std::string>> s_unlocalizedNames;

    Lang::Lang(std::string_view unlocalizedString) : m_entryHash(LangConst::hash(unlocalizedString)) {
        if (!s_unlocalizedNames->contains(m_entryHash)) [[unlikely]] {
            s_unlocalizedNames->emplace(m_entryHash, unlocalizedString);
        }
    }
    Lang::Lang(const char *unlocalizedString) : Lang(std::string_view(unlocalizedString)) { }
    Lang::Lang(const std::string &unlocalizedString) : Lang(std::string_view(unlocalizedString)) { }
    Lang::Lang(const LangConst &localizedString) : m_entryHash(localizedString.m_entryHash) {
        if (!s_unlocalizedNames->contains(m_entryHash)) [[unlikely]] {
            s_unlocalizedNames->emplace(m_entryHash, localizedString.m_unlocalizedString);
        }
    }
    Lang::Lang(const UnlocalizedString &unlocalizedString) : Lang(unlocalizedString.get()) { }

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
            if (auto unlocalizedIt = s_unlocalizedNames->find(m_entryHash); unlocalizedIt != s_unlocalizedNames->end()) {
                return unlocalizedIt->second.c_str();
            } else {
                return "<unlocalized>";
            }
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
