#pragma once

#include <hex.hpp>

#include <hex/api/task_manager.hpp>
#include <hex/ui/view.hpp>
#include <hex/helpers/binary_pattern.hpp>
#include <ui/widgets.hpp>

#include <vector>

#include <wolv/container/interval_tree.hpp>

#include <hex/api/content_registry.hpp>

namespace hex::plugin::builtin {

    class ViewFind : public View::Window {
    public:
        ViewFind();
        ~ViewFind() override = default;

        void drawContent() override;

    private:

        using Occurrence = hex::ContentRegistry::DataFormatter::impl::FindOccurrence;

        struct BinaryPattern {
            u8 mask, value;
        };

        struct SearchSettings {
            ui::RegionType range = ui::RegionType::EntireData;
            Region region = { 0, 0 };

            enum class Mode : int {
                Strings,
                Sequence,
                Regex,
                BinaryPattern,
                Value
            } mode = Mode::Strings;

            enum class StringType : int { ASCII = 0, UTF8 = 1, UTF16LE = 2, UTF16BE = 3, ASCII_UTF16LE = 4, ASCII_UTF16BE = 5 };

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

                StringType type = StringType::ASCII;
                bool ignoreCase = false;
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
                hex::BinaryPattern pattern;
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

        using OccurrenceTree = wolv::container::IntervalTree<Occurrence>;

        PerProvider<std::vector<Occurrence>> m_foundOccurrences, m_sortedOccurrences;
        PerProvider<Occurrence*> m_lastSelectedOccurrence;
        PerProvider<OccurrenceTree> m_occurrenceTree;
        PerProvider<std::string> m_currFilter;

        TaskHolder m_searchTask, m_filterTask;
        bool m_settingsValid = false;
        std::string m_replaceBuffer;

    private:
        static std::vector<Occurrence> searchStrings(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Strings &settings);
        static std::vector<Occurrence> searchSequence(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Sequence &settings);
        static std::vector<Occurrence> searchRegex(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Regex &settings);
        static std::vector<Occurrence> searchBinaryPattern(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::BinaryPattern &settings);
        static std::vector<Occurrence> searchValue(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Value &settings);

        void drawContextMenu(Occurrence &target, const std::string &value);

        static std::vector<BinaryPattern> parseBinaryPatternString(std::string string);
        static std::tuple<bool, std::variant<u64, i64, float, double>, size_t> parseNumericValueInput(const std::string &input, SearchSettings::Value::Type type);

        void runSearch();
        std::string decodeValue(prv::Provider *provider, const Occurrence &occurrence, size_t maxBytes = 0xFFFF'FFFF) const;
    };

}