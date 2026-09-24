#pragma once

#include <hex.hpp>

#include <hex/api/task_manager.hpp>
#include <hex/ui/view.hpp>
#include <hex/helpers/binary_pattern.hpp>
#include <ui/widgets.hpp>

#include <vector>

#include <wolv/container/interval_tree.hpp>

#include <hex/api/content_registry/views.hpp>
#include <hex/api/content_registry/data_formatter.hpp>

namespace hex::plugin::builtin {

    struct FindOccurrence {
        Region region;
        std::endian endian = std::endian::native;
        enum class DecodeType : u8 { ASCII, UTF8, Binary, UTF16, Unsigned, Signed, Float, Double } decodeType;
        bool selected;
        std::string string;
    };

    class ViewFind : public View::Window {
    public:
        ViewFind();
        ~ViewFind() override = default;

        void drawContent() override;

        View* getMenuItemInheritView() const override {
            return ContentRegistry::Views::getViewByName("hex.builtin.view.hex_editor.name"_unlocalized);
        }

        void drawHelpText() override;

    private:
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
                Value,
                Constants
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

            struct Constants {
                u32 alignment = 1;
            } constants;

        } m_searchSettings, m_decodeSettings;

        using OccurrenceTree = wolv::container::IntervalTree<FindOccurrence>;

        PerProvider<std::vector<FindOccurrence>> m_foundOccurrences, m_sortedOccurrences;
        PerProvider<FindOccurrence*> m_lastSelectedOccurrence;
        PerProvider<OccurrenceTree> m_occurrenceTree;
        PerProvider<std::string> m_currFilter;
        PerProvider<bool> m_settingsCollapsed;

        /**
         * @brief The maximum number of bytes of a value that the result table shows
         */
        constexpr static size_t MaxDisplayedValueSize = 256;

        /**
         * @brief A result table column to sort on
         */
        enum class SortColumn { Offset, Size, Value };

        /**
         * @brief A sort order of the result table
         */
        struct SortOrder {
            SortColumn column = SortColumn::Offset;
            bool ascending = true;
        } m_sortOrder;

        TaskHolder m_searchTask, m_filterTask;
        bool m_settingsValid = false;
        std::string m_replaceBuffer;

    private:
        static std::vector<FindOccurrence> searchStrings(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Strings &settings);
        static std::vector<FindOccurrence> searchSequence(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Sequence &settings);
        static std::vector<FindOccurrence> searchRegex(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Regex &settings);
        static std::vector<FindOccurrence> searchBinaryPattern(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::BinaryPattern &settings);
        static std::vector<FindOccurrence> searchValue(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Value &settings);
        static std::vector<FindOccurrence> searchConstants(Task &task, prv::Provider *provider, Region searchRegion, const SearchSettings::Constants &settings);

        void drawContextMenu(FindOccurrence& target, const std::string &value);

        static std::vector<BinaryPattern> parseBinaryPatternString(std::string string);
        static std::tuple<bool, std::variant<u64, i64, float, double>, size_t> parseNumericValueInput(const std::string &input, SearchSettings::Value::Type type);

        void runSearch();
        std::string decodeValue(prv::Provider *provider, const FindOccurrence& occurrence, size_t maxBytes) const;

        /**
         * @brief Sorts occurrences
         * @param provider The provider to read the values from
         * @param occurrences The occurrences to sort
         * @param sortOrder The sort order
         * @note Sorts values by raw bytes and constants by name. It does not decode values.
         */
        void sortOccurrences(prv::Provider *provider, std::vector<FindOccurrence> &occurrences, const SortOrder &sortOrder) const;
    };

}
