#pragma once

#include <initializer_list>
#include <map>
#include <string>
#include <string_view>

#include <fmt/format.h>

namespace hex {

    class LanguageDefinition {
    public:
        explicit LanguageDefinition(std::map<std::string, std::string> &&entries);

        [[nodiscard]] const std::map<std::string, std::string> &getEntries() const;

    private:
        std::map<std::string, std::string> m_entries;
    };

    class LangEntry {
    public:
        explicit LangEntry(const char *unlocalizedString);
        explicit LangEntry(std::string unlocalizedString);
        explicit LangEntry(std::string_view unlocalizedString);

        operator std::string() const;
        operator std::string_view() const;
        operator const char *() const;

        [[nodiscard]] const std::string &get() const;

        static void loadLanguage(const std::string &language);
        static const std::map<std::string, std::string> &getSupportedLanguages();

        static void setFallbackLanguage(const std::string &language);
        static const std::string &getFallbackLanguage();

        static void resetLanguageStrings();

        static const std::string &getSelectedLanguage();

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

    inline LangEntry operator""_lang(const char *string, size_t) {
        return LangEntry(string);
    }

}

template<>
struct fmt::formatter<hex::LangEntry> : fmt::formatter<std::string_view> {
    template<typename FormatContext>
    auto format(const hex::LangEntry &entry, FormatContext &ctx) {
        return fmt::formatter<std::string_view>::format(entry.get(), ctx);
    }
};