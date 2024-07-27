#pragma once

#include <hex.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
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

    class Lang {
    public:
        explicit Lang(const char *unlocalizedString);
        explicit Lang(const std::string &unlocalizedString);
        explicit Lang(const UnlocalizedString &unlocalizedString);
        explicit Lang(std::string_view unlocalizedString);

        [[nodiscard]] operator std::string() const;
        [[nodiscard]] operator std::string_view() const;
        [[nodiscard]] operator const char *() const;

        const std::string &get() const;

        constexpr static size_t hash(std::string_view string){
            constexpr u64 p = 131;
            constexpr u64 m = std::numeric_limits<std::uint32_t>::max() - 4; // Largest 32 bit prime
            u64 total = 0;
            u64 currentMultiplier = 1;

            for (char c : string) {
                total = (total + currentMultiplier * c) % m;
                currentMultiplier = (currentMultiplier * p) % m;
            }

            return total;
        }

    private:
        constexpr explicit Lang(std::size_t hash) : m_entryHash(hash) {}

        template<wolv::type::StaticString>
        friend consteval Lang operator""_lang();

    private:
        std::size_t m_entryHash;
    };

    [[nodiscard]] std::string operator+(const std::string &&left, const Lang &&right);
    [[nodiscard]] std::string operator+(const Lang &&left, const std::string &&right);
    [[nodiscard]] std::string operator+(const std::string_view &&left, const Lang &&right);
    [[nodiscard]] std::string operator+(const Lang &&left, const std::string_view &&right);
    [[nodiscard]] std::string operator+(const char *left, const Lang &&right);
    [[nodiscard]] std::string operator+(const Lang &&left, const char *right);
    [[nodiscard]] std::string operator+(const Lang &&left, const Lang &&right);

    template<wolv::type::StaticString String>
    [[nodiscard]] consteval Lang operator""_lang() {
        return Lang(Lang::hash(String.value.data()));
    }


    struct UnlocalizedString {
    public:
        UnlocalizedString() = default;

        UnlocalizedString(auto && arg) : m_unlocalizedString(std::forward<decltype(arg)>(arg)) {
            static_assert(!std::same_as<std::remove_cvref_t<decltype(arg)>, Lang>, "Expected a unlocalized name, got a localized one!");
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

    // {fmt} formatter for hex::Lang
    inline auto format_as(const hex::Lang &entry) {
        return entry.get();
    }

}
