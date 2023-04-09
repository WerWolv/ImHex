#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>
#include <ui/widgets.hpp>

#include <atomic>
#include <vector>

#include <IntervalTree.h>

namespace hex::plugin::builtin {

    class ViewFind : public View {
    public:
        ViewFind();
        ~ViewFind() override = default;

        void drawContent() override;

    private:

        struct Occurrence {
            Region region;
            enum class DecodeType { ASCII, Binary, UTF16, Unsigned, Signed, Float, Double } decodeType;
            std::endian endian = std::endian::native;
        };

        struct BinaryPattern {
            u8 mask, value;
        };

        struct SearchSettings {
            ui::SelectedRegion range = ui::SelectedRegion::EntireData;

            enum class Mode : int {
                Strings,
                Sequence,
                Regex,
                BinaryPattern,
                Value
            } mode = Mode::Strings;

            enum class StringType : int { ASCII = 0, UTF16LE = 1, UTF16BE = 2, ASCII_UTF16LE = 3, ASCII_UTF16BE = 4 };

            struct Strings {
                int minLength = 5;
                bool nullTermination = false;
                StringType type = StringType::ASCII;

                bool lowerCaseLetters = true;
                bool upperCaseLetters = true;
                bool numbers = true;
                bool underscores = true;
                bool symbols = true;
                bool spaces = true;
                bool lineFeeds = false;
            } strings;

            struct Sequence {
                std::string sequence;
            } bytes;

            struct Regex {
                int minLength = 5;
                bool nullTermination = false;
                StringType type = StringType::ASCII;

                std::string pattern;
                bool fullMatch = true;
            } regex;

            struct BinaryPattern {
                std::string input;
                std::vector<ViewFind::BinaryPattern> pattern;
                u32 alignment = 1;
            } binaryPattern;

            struct Value {
                std::string inputMin, inputMax;
                std::endian endian = std::endian::native;
                bool aligned = false;
                bool range = false;

                enum class Type {
                    U8 = 0, U16 = 1, U32 = 2, U64 = 3,
                    I8 = 4, I16 = 5, I32 = 6, I64 = 7,
                    F32 = 8, F64 = 9
                } type = Type::U8;
            } value;

        } m_searchSettings, m_decodeSettings;

        using OccurrenceTree = interval_tree::IntervalTree<u64, Occurrence>;

        std::map<prv::Provider*, std::vector<Occurrence>> m_foundOccurrences, m_sortedOccurrences;
        std::map<prv::Provider*, OccurrenceTree> m_occurrenceTree;
        std::map<prv::Provider*, std::string> m_currFilter;

        TaskHolder m_searchTask, m_filterTask;
        bool m_settingsValid = false;

    private:
        static std::vector<Occurrence> searchStrings(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Strings &settings);
        static std::vector<Occurrence> searchSequence(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Sequence &settings);
        static std::vector<Occurrence> searchRegex(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Regex &settings);
        static std::vector<Occurrence> searchBinaryPattern(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::BinaryPattern &settings);
        static std::vector<Occurrence> searchValue(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Value &settings);

        static std::vector<BinaryPattern> parseBinaryPatternString(std::string string);
        static std::tuple<bool, std::variant<u64, i64, float, double>, size_t> parseNumericValueInput(const std::string &input, SearchSettings::Value::Type type);

        void runSearch();
        std::string decodeValue(prv::Provider *provider, Occurrence occurrence, size_t maxBytes = 0xFFFF'FFFF) const;
    };

}