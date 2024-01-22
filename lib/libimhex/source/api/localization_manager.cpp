#include <hex/api/content_registry.hpp>
#include <hex/api/localization_manager.hpp>

namespace hex {

    namespace LocalizationManager {

        namespace {

            std::string s_fallbackLanguage;
            std::string s_selectedLanguage;
            std::map<std::string, std::string> s_currStrings;

        }

        namespace impl {

            void resetLanguageStrings() {
                s_currStrings.clear();
                s_selectedLanguage.clear();
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

        void loadLanguage(const std::string &language) {
            s_currStrings.clear();

            auto &definitions = ContentRegistry::Language::impl::getLanguageDefinitions();

            if (!definitions.contains(language))
                return;

            for (auto &definition : definitions[language])
                s_currStrings.insert(definition.getEntries().begin(), definition.getEntries().end());

            const auto& fallbackLanguage = getFallbackLanguage();
            if (language != fallbackLanguage) {
                for (auto &definition : definitions[fallbackLanguage])
                    s_currStrings.insert(definition.getEntries().begin(), definition.getEntries().end());
            }

            s_selectedLanguage = language;
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

    Lang::Lang(const char *unlocalizedString) : m_unlocalizedString(unlocalizedString) { }
    Lang::Lang(const std::string &unlocalizedString) : m_unlocalizedString(unlocalizedString) { }
    Lang::Lang(const UnlocalizedString &unlocalizedString) : m_unlocalizedString(unlocalizedString.get()) { }
    Lang::Lang(std::string_view unlocalizedString) : m_unlocalizedString(unlocalizedString) { }


    Lang::operator std::string() const {
        return get();
    }

    Lang::operator std::string_view() const {
        return get();
    }

    Lang::operator const char *() const {
        return get().c_str();
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

    const std::string &Lang::get() const {
        auto &lang = LocalizationManager::s_currStrings;
        if (lang.contains(m_unlocalizedString))
            return lang[m_unlocalizedString];
        else
            return m_unlocalizedString;
    }

}