#pragma once

#include <map>
#include <string>
#include <string_view>

namespace hex {

    class LangEntry {
    public:
        LangEntry(const char *unlocalizedString);

        operator std::string() const;
        operator std::string_view() const;
        operator const char*() const;

        std::string_view get() const;

        static void loadLanguage(std::string_view language);
        static const std::map<std::string, std::string>& getSupportedLanguages();

    private:
        std::string m_unlocalizedString;
    };

    namespace lang_literals {

        inline LangEntry operator""_lang(const char *string, size_t) {
            return LangEntry(string);
        }

    }



}