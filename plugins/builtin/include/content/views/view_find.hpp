#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>

#include <atomic>
#include <vector>

namespace hex::plugin::builtin {

    class ViewFind : public View {
    public:
        ViewFind();
        ~ViewFind() override = default;

        void drawContent() override;

    private:

        struct SearchSettings {
            int range = 0;

            enum class Mode : int {
                Strings,
                Sequence,
                Regex
            } mode = Mode::Strings;

            struct Strings {
                int minLength = 5;
                enum class Type : int { ASCII = 0, UTF16LE = 1, UTF16BE = 2 } type = Type::ASCII;
                bool nullTermination = false;

                bool m_lowerCaseLetters = true;
                bool m_upperCaseLetters = true;
                bool m_numbers = true;
                bool m_underscores = true;
                bool m_symbols = true;
                bool m_spaces = true;
                bool m_lineFeeds = false;
            } strings;

            struct Bytes {
                std::string sequence;
            } bytes;

            struct Regex {
                std::string pattern;
            } regex;
        } m_searchSettings, m_decodeSettings;

        std::map<prv::Provider*, std::vector<Region>> m_foundRegions, m_sortedRegions;
        std::atomic<bool> m_searchRunning;
        bool m_settingsValid = false;

    private:
        static std::vector<Region> searchStrings(prv::Provider *provider, Region searchRegion, SearchSettings::Strings settings);
        static std::vector<Region> searchSequence(prv::Provider *provider, Region searchRegion, SearchSettings::Bytes settings);
        static std::vector<Region> searchRegex(prv::Provider *provider, Region searchRegion, SearchSettings::Regex settings);

        void runSearch();
        std::string decodeValue(prv::Provider *provider, Region region);
    };

}