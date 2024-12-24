#pragma once

#include <hex.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/core.h>
#include <wolv/types/static_string.hpp>

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
        std::string getLocalizedString(const std::string &unlocalizedString, const std::string &language = "");

        [[nodiscard]] const std::map<std::string, std::string> &getSupportedLanguages();
        [[nodiscard]] const std::string &getFallbackLanguage();
        [[nodiscard]] const std::string &getSelectedLanguage();

    }

    struct UnlocalizedString;

    class LangConst;

    class Lang {
    public:
        explicit Lang(const char *unlocalizedString);
        explicit Lang(const std::string &unlocalizedString);
        explicit(false) Lang(const LangConst &localizedString);
        explicit Lang(const UnlocalizedString &unlocalizedString);
        explicit Lang(std::string_view unlocalizedString);

        [[nodiscard]] operator std::string() const;
        [[nodiscard]] operator std::string_view() const;
        [[nodiscard]] operator const char *() const;

        const char* get() const;

    private:
        std::size_t m_entryHash;
        std::string m_unlocalizedString;
    };

    class LangConst {
    public:
        [[nodiscard]] operator std::string() const;
        [[nodiscard]] operator std::string_view() const;
        [[nodiscard]] operator const char *() const;

        const char* get() const;

        constexpr static size_t hash(std::string_view string) {
            constexpr u64 p = 131;
            constexpr u64 m = std::numeric_limits<std::uint32_t>::max() - 4;
            u64 total = 0;
            u64 currentMultiplier = 1;

            for (char c : string) {
                total = (total + currentMultiplier * c) % m;
                currentMultiplier = (currentMultiplier * p) % m;
            }

            return total;
        }

    private:
        constexpr explicit LangConst(std::size_t hash, const char *unlocalizedString) : m_entryHash(hash), m_unlocalizedString(unlocalizedString) {}

        template<wolv::type::StaticString>
        friend consteval LangConst operator""_lang();
        friend class Lang;

    private:
        std::size_t m_entryHash;
        const char *m_unlocalizedString = nullptr;
    };

    struct UnlocalizedString {
    public:
        UnlocalizedString() = default;

        template<typename T>
        UnlocalizedString(T &&arg) : m_unlocalizedString(std::forward<T>(arg)) {
            static_assert(!std::same_as<std::remove_cvref_t<T>, Lang>, "Expected a unlocalized name, got a localized one!");
        }

        [[nodiscard]] operator std::string() const {
            return m_unlocalizedString;
        }

        [[nodiscard]] operator std::string_view() const {
            return m_unlocalizedString;
        }

        [[nodiscard]] operator const char *() const {
            return m_unlocalizedString.c_str();
        }

        [[nodiscard]] const std::string &get() const {
            return m_unlocalizedString;
        }

        [[nodiscard]] bool empty() const {
            return m_unlocalizedString.empty();
        }

        auto operator<=>(const UnlocalizedString &) const = default;
        auto operator<=>(const std::string &other) const {
            return m_unlocalizedString <=> other;
        }

    private:
        std::string m_unlocalizedString;
    };

    template<wolv::type::StaticString String>
    [[nodiscard]] consteval LangConst operator""_lang() {
        return LangConst(LangConst::hash(String.value.data()), String.value.data());
    }

    // {fmt} formatter for hex::Lang and hex::LangConst
    inline auto format_as(const hex::Lang &entry) {
        return entry.get();
    }
    inline auto format_as(const hex::LangConst &entry) {
        return entry.get();
    }

}
