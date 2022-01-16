#pragma once

#include <initializer_list>
#include <map>
#include <string>
#include <string_view>

namespace hex {

    class LanguageDefinition {
    public:
        LanguageDefinition(std::initializer_list<std::pair<std::string, std::string>> entries);

        const std::map<std::string, std::string>& getEntries() const;

    private:
        std::map<std::string, std::string> m_entries;
    };

    class LangEntry {
    public:
        explicit LangEntry(const char *unlocalizedString);
        explicit LangEntry(const std::string &unlocalizedString);
        explicit LangEntry(std::string_view unlocalizedString);

        operator std::string() const;
        operator std::string_view() const;
        operator const char*() const;

        [[nodiscard]] std::string_view get() const;

        static void loadLanguage(std::string_view language);
        static const std::map<std::string, std::string>& getSupportedLanguages();

    private:
        std::string m_unlocalizedString;
    };

    std::string operator+(const std::string &&left, const LangEntry &&right);
    std::string operator+(const LangEntry &&left, const std::string &&right);
    std::string operator+(const std::string_view &&left, const LangEntry &&right);
    std::string operator+(const LangEntry &&left, const std::string_view &&right);
    std::string operator+(const char *left, const LangEntry &&right);
    std::string operator+(const LangEntry &&left, const char *right);
    std::string operator+(const LangEntry &&left, const LangEntry &&right);

    namespace lang_literals {

        inline LangEntry operator""_lang(const char *string, size_t) {
            return LangEntry(string);
        }

    }



}