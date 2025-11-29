#pragma once

#include <hex.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <functional>

#include <fmt/core.h>
#include <wolv/types/static_string.hpp>

EXPORT_MODULE namespace hex {

    struct UnlocalizedString;
    using LanguageId = std::string;

    namespace LocalizationManager {

        struct PathEntry {
            std::string path;
            std::function<std::string_view(const std::string &path)> callback;
        };

        struct LanguageDefinition {
            LanguageId id;
            std::string name, nativeName;
            LanguageId fallbackLanguageId;
            bool hidden;

            std::vector<PathEntry> languageFilePaths;
        };

        void addLanguages(const std::string_view &languageList, std::function<std::string_view(const std::string &path)> callback);
        void setLanguage(const LanguageId &languageId);
        [[nodiscard]] const LanguageId& getSelectedLanguageId();
        [[nodiscard]] const std::string& get(const LanguageId& languageId, const UnlocalizedString &unlocalizedString);
        [[nodiscard]] const std::map<LanguageId, LanguageDefinition>& getLanguageDefinitions();
        [[nodiscard]] const LanguageDefinition& getLanguageDefinition(const LanguageId &languageId);

    }

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

        UnlocalizedString(const std::string &string) : m_unlocalizedString(string) { }
        UnlocalizedString(const char *string) : m_unlocalizedString(string) { }
        UnlocalizedString(const Lang& arg) = delete;
        UnlocalizedString(std::string &&string) : m_unlocalizedString(std::move(string)) { }
        UnlocalizedString(UnlocalizedString &&) = default;
        UnlocalizedString(const UnlocalizedString &) = default;

        UnlocalizedString &operator=(const UnlocalizedString &) = default;
        UnlocalizedString &operator=(UnlocalizedString &&) = default;
        UnlocalizedString &operator=(const std::string &string) { m_unlocalizedString = string; return *this; }
        UnlocalizedString &operator=(std::string &&string) { m_unlocalizedString = std::move(string); return *this; }

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

template<>
struct std::hash<hex::UnlocalizedString> {
    std::size_t operator()(const hex::UnlocalizedString &string) const noexcept {
        return std::hash<std::string>{}(string.get());
    }
};

namespace fmt {

    template<typename ... Args>
    auto format(const hex::Lang &entry, Args &&... args) {
        return fmt::format(fmt::runtime(entry.get()), std::forward<Args>(args)...);
    }

}