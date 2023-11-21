#pragma once

#include <map>
#include <string>
#include <string_view>

#include <fmt/format.h>

namespace hex {

    namespace LocalizationManager {

        class LanguageDefinition {
        public:
            explicit LanguageDefinition(std::map<std::string, std::string> &&entries);

            [[nodiscard]] const std::map<std::string, std::string> &getEntries() const;

        private:
            std::map<std::string, std::string> m_entries;
        };

        namespace impl {
            void setFallbackLanguage(const std::string &language);
            void resetLanguageStrings();
        }

        void loadLanguage(const std::string &language);

        [[nodiscard]] const std::map<std::string, std::string> &getSupportedLanguages();
        [[nodiscard]] const std::string &getFallbackLanguage();
        [[nodiscard]] const std::string &getSelectedLanguage();
    }

    class Lang {
    public:
        explicit Lang(const char *unlocalizedString);
        explicit Lang(std::string unlocalizedString);
        explicit Lang(std::string_view unlocalizedString);

        [[nodiscard]] operator std::string() const;
        [[nodiscard]] operator std::string_view() const;
        [[nodiscard]] operator const char *() const;

        [[nodiscard]] const std::string &get() const;

    private:
        std::string m_unlocalizedString;
    };

    [[nodiscard]] std::string operator+(const std::string &&left, const Lang &&right);
    [[nodiscard]] std::string operator+(const Lang &&left, const std::string &&right);
    [[nodiscard]] std::string operator+(const std::string_view &&left, const Lang &&right);
    [[nodiscard]] std::string operator+(const Lang &&left, const std::string_view &&right);
    [[nodiscard]] std::string operator+(const char *left, const Lang &&right);
    [[nodiscard]] std::string operator+(const Lang &&left, const char *right);
    [[nodiscard]] std::string operator+(const Lang &&left, const Lang &&right);

    [[nodiscard]] inline Lang operator""_lang(const char *string, size_t) {
        return Lang(string);
    }

}

template<>
struct fmt::formatter<hex::Lang> : fmt::formatter<std::string_view> {
    template<typename FormatContext>
    auto format(const hex::Lang &entry, FormatContext &ctx) {
        return fmt::formatter<std::string_view>::format(entry.get(), ctx);
    }
};