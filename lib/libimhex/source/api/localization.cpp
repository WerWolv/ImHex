#include <hex/api/localization.hpp>

#include <hex/api/content_registry.hpp>

namespace hex {

    std::string LangEntry::s_fallbackLanguage;
    std::map<std::string, std::string> LangEntry::s_currStrings;

    LanguageDefinition::LanguageDefinition(std::map<std::string, std::string> &&entries) {
        for (const auto &[key, value] : entries) {
            if (value.empty())
                continue;

            this->m_entries.insert({ key, value });
        }

    }

    const std::map<std::string, std::string> &LanguageDefinition::getEntries() const {
        return this->m_entries;
    }

    LangEntry::LangEntry(const char *unlocalizedString) : m_unlocalizedString(unlocalizedString) { }
    LangEntry::LangEntry(std::string unlocalizedString) : m_unlocalizedString(std::move(unlocalizedString)) { }
    LangEntry::LangEntry(std::string_view unlocalizedString) : m_unlocalizedString(unlocalizedString) { }

    LangEntry::operator std::string() const {
        return get();
    }

    LangEntry::operator std::string_view() const {
        return get();
    }

    LangEntry::operator const char *() const {
        return get().c_str();
    }

    std::string operator+(const std::string &&left, const LangEntry &&right) {
        return left + static_cast<std::string>(right);
    }

    std::string operator+(const LangEntry &&left, const std::string &&right) {
        return static_cast<std::string>(left) + right;
    }

    std::string operator+(const LangEntry &&left, const LangEntry &&right) {
        return static_cast<std::string>(left) + static_cast<std::string>(right);
    }

    std::string operator+(const std::string_view &&left, const LangEntry &&right) {
        return std::string(left) + static_cast<std::string>(right);
    }

    std::string operator+(const LangEntry &&left, const std::string_view &&right) {
        return static_cast<std::string>(left) + std::string(right);
    }

    std::string operator+(const char *left, const LangEntry &&right) {
        return left + static_cast<std::string>(right);
    }

    std::string operator+(const LangEntry &&left, const char *right) {
        return static_cast<std::string>(left) + right;
    }

    const std::string &LangEntry::get() const {
        auto &lang = LangEntry::s_currStrings;
        if (lang.contains(this->m_unlocalizedString))
            return lang[this->m_unlocalizedString];
        else
            return this->m_unlocalizedString;
    }

    void LangEntry::loadLanguage(const std::string &language) {
        LangEntry::s_currStrings.clear();

        auto &definitions = ContentRegistry::Language::getLanguageDefinitions();

        if (!definitions.contains(language))
            return;

        for (auto &definition : definitions[language])
            LangEntry::s_currStrings.insert(definition.getEntries().begin(), definition.getEntries().end());

        const auto fallbackLanguage = LangEntry::getFallbackLanguage();
        if (language != fallbackLanguage) {
            for (auto &definition : definitions[fallbackLanguage])
                LangEntry::s_currStrings.insert(definition.getEntries().begin(), definition.getEntries().end());
        }
    }

    const std::map<std::string, std::string> &LangEntry::getSupportedLanguages() {
        return ContentRegistry::Language::getLanguages();
    }

    void LangEntry::setFallbackLanguage(const std::string &language) {
        LangEntry::s_fallbackLanguage = language;
    }

    const std::string &LangEntry::getFallbackLanguage() {
        return LangEntry::s_fallbackLanguage;
    }

    void LangEntry::resetLanguageStrings() {
        LangEntry::s_currStrings.clear();
    }

}