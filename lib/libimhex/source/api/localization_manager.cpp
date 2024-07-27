#include <hex/api/content_registry.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/helpers/auto_reset.hpp>

namespace hex {

    namespace LocalizationManager {

        namespace {

            AutoReset<std::string> s_fallbackLanguage;
            AutoReset<std::string> s_selectedLanguage;
            AutoReset<std::map<size_t, std::string>> s_currStrings;

        }

        namespace impl {

            void resetLanguageStrings() {
                s_currStrings->clear();
                s_selectedLanguage->clear();
            }

            void setFallbackLanguage(const std::string &language) {
                s_fallbackLanguage = language;
            }

        }

        LanguageDefinition::LanguageDefinition(std::map<std::string, std::string> &&entries) {
            for (const auto &[key, value] : entries) {
                if (value.empty())
                    continue;

                m_entries.insert({ key, value });
            }

        }

        const std::map<std::string, std::string> &LanguageDefinition::getEntries() const {
            return m_entries;
        }

        static void loadLanguageDefinitions(const std::vector<LanguageDefinition> &definitions) {
            for (const auto &definition : definitions) {
                const auto &entries = definition.getEntries();
                if (entries.empty())
                    continue;

                for (const auto &[key, value] : entries) {
                    if (value.empty())
                        continue;

                    s_currStrings->emplace(Lang::hash(key), value);
                }
            }
        }

        void loadLanguage(const std::string &language) {
            auto &definitions = ContentRegistry::Language::impl::getLanguageDefinitions();

            if (!definitions.contains(language))
                return;

            s_currStrings->clear();

            loadLanguageDefinitions(definitions.at(language));

            const auto& fallbackLanguage = getFallbackLanguage();
            loadLanguageDefinitions(definitions.at(fallbackLanguage));

            s_selectedLanguage = language;
        }

        std::string getLocalizedString(const std::string& unlocalizedString, const std::string& language) {
            if (language.empty())
                return getLocalizedString(unlocalizedString, getSelectedLanguage());

            auto &languageDefinitions = ContentRegistry::Language::impl::getLanguageDefinitions();
            if (!languageDefinitions.contains(language))
                return "";

            std::string localizedString;
            for (const auto &definition : languageDefinitions.at(language)) {
                if (definition.getEntries().contains(unlocalizedString)) {
                    localizedString = definition.getEntries().at(unlocalizedString);
                    break;
                }
            }

            if (localizedString.empty())
                return getLocalizedString(unlocalizedString, getFallbackLanguage());

            return localizedString;
        }


        const std::map<std::string, std::string> &getSupportedLanguages() {
            return ContentRegistry::Language::impl::getLanguages();
        }

        const std::string &getFallbackLanguage() {
            return s_fallbackLanguage;
        }

        const std::string &getSelectedLanguage() {
            return s_selectedLanguage;
        }

    }

    Lang::Lang(const char *unlocalizedString) : m_entryHash(hash(unlocalizedString)) { }
    Lang::Lang(const std::string &unlocalizedString) : m_entryHash(hash(unlocalizedString)) { }
    Lang::Lang(const UnlocalizedString &unlocalizedString) : m_entryHash(hash(unlocalizedString.get())) { }
    Lang::Lang(std::string_view unlocalizedString) : m_entryHash(hash(unlocalizedString)) { }

    Lang::operator std::string() const {
        return get();
    }

    Lang::operator std::string_view() const {
        return get();
    }

    Lang::operator const char *() const {
        return get().c_str();
    }

    const std::string &Lang::get() const {
        const auto &lang = *LocalizationManager::s_currStrings;

        auto it = lang.find(m_entryHash);
        if (it == lang.end()) {
            static const std::string invalidString = "[ !!! INVALID LANGUAGE STRING !!! ]";
            return invalidString;
        } else {
            return it->second;
        }
    }

    std::string operator+(const std::string &&left, const Lang &&right) {
        return left + static_cast<std::string>(right);
    }

    std::string operator+(const Lang &&left, const std::string &&right) {
        return static_cast<std::string>(left) + right;
    }

    std::string operator+(const Lang &&left, const Lang &&right) {
        return static_cast<std::string>(left) + static_cast<std::string>(right);
    }

    std::string operator+(const std::string_view &&left, const Lang &&right) {
        return std::string(left) + static_cast<std::string>(right);
    }

    std::string operator+(const Lang &&left, const std::string_view &&right) {
        return static_cast<std::string>(left) + std::string(right);
    }

    std::string operator+(const char *left, const Lang &&right) {
        return left + static_cast<std::string>(right);
    }

    std::string operator+(const Lang &&left, const char *right) {
        return static_cast<std::string>(left) + right;
    }

}