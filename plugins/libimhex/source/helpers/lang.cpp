#include "hex/helpers/lang.hpp"

#include "hex/helpers/shared_data.hpp"

namespace hex {

    LanguageDefinition::LanguageDefinition(std::initializer_list<std::pair<std::string, std::string>> entries) {
        for (auto pair : entries)
            this->m_entries.insert(pair);
    }

    const std::map<std::string, std::string>& LanguageDefinition::getEntries() const {
        return this->m_entries;
    }

    LangEntry::LangEntry(const char *unlocalizedString) : m_unlocalizedString(unlocalizedString) { }
    LangEntry::LangEntry(const std::string &unlocalizedString) : m_unlocalizedString(unlocalizedString) { }
    LangEntry::LangEntry(std::string_view unlocalizedString) : m_unlocalizedString(unlocalizedString) { }

    LangEntry::operator std::string() const {
        return std::string(get());
    }

    LangEntry::operator std::string_view() const {
        return get();
    }

    LangEntry::operator const char*() const {
        return get().data();
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

    std::string_view LangEntry::get() const {
        auto &lang = SharedData::loadedLanguageStrings;
        if (lang.find(this->m_unlocalizedString) != lang.end())
            return lang[this->m_unlocalizedString];
        else
            return this->m_unlocalizedString;
    }

    void LangEntry::loadLanguage(std::string_view language) {
        constexpr auto DefaultLanguage = "en-US";

        SharedData::loadedLanguageStrings.clear();

        auto &definitions = ContentRegistry::Language::getLanguageDefinitions();

        if (!definitions.contains(language.data()))
            return;

        for (auto &definition : definitions[language.data()])
            SharedData::loadedLanguageStrings.insert(definition.getEntries().begin(), definition.getEntries().end());

        if (language != DefaultLanguage) {
            for (auto &definition : definitions[DefaultLanguage])
                SharedData::loadedLanguageStrings.insert(definition.getEntries().begin(), definition.getEntries().end());
        }
    }

    const std::map<std::string, std::string>& LangEntry::getSupportedLanguages() {
        return ContentRegistry::Language::getLanguages();
    }

}