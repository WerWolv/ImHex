#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <bit>

EXPORT_MODULE namespace hex {

    /* Data Inspector Registry. Allows adding of new types to the data inspector */
    namespace ContentRegistry::DataInspector {

        enum class NumberDisplayStyle : u8 {
            Decimal,
            Hexadecimal,
            Octal
        };

        namespace impl {

            struct DoNotUseThisByItselfTag {};

            using DisplayFunction   = std::function<std::string()>;
            using EditingFunction   = std::function<std::optional<std::vector<u8>>(std::string&, std::endian, DoNotUseThisByItselfTag)>;
            using GeneratorFunction = std::function<DisplayFunction(const std::vector<u8> &, std::endian, NumberDisplayStyle)>;

            /**
             * @brief Reports how many of the generator function's bytes a row actually used
             *
             * Applies to a row whose requiredSize and maxSize differ, for example
             * a string row that reads up to a display budget but a shorter
             * selection ends the string sooner, or a UTF-16 code point that
             * reads 2 or 4 bytes depending on whether it read a surrogate pair.
             */
            using SizeFunction = std::function<size_t(const std::vector<u8> &, std::endian)>;

            struct Entry {
                UnlocalizedString unlocalizedName;
                size_t requiredSize;
                size_t maxSize;
                GeneratorFunction generatorFunction;
                std::optional<EditingFunction> editingFunction;
                std::optional<SizeFunction> sizeFunction;
            };

            const std::vector<Entry>& getEntries();

        }

        namespace EditWidget {

            class Widget {
            public:
                /**
                 * @brief Encodes a row's typed text into raw bytes
                 * @return The bytes, or nullopt if the text is not a valid edit
                 */
                using Function = std::function<std::optional<std::vector<u8>>(const std::string&, std::endian)>;

                explicit Widget(const Function &function) : m_function(function) {}

                virtual ~Widget() = default;
                virtual std::optional<std::vector<u8>> draw(std::string &value, std::endian endian) = 0;
                std::optional<std::vector<u8>> operator()(std::string &value, std::endian endian, impl::DoNotUseThisByItselfTag) {
                    return draw(value, endian);
                }

                std::optional<std::vector<u8>> getBytes(const std::string &value, std::endian endian) const {
                    return m_function(value, endian);
                }

            private:
                Function m_function;
            };

            struct TextInput : Widget {
                explicit TextInput(const Function &function) : Widget(function) {}
                std::optional<std::vector<u8>> draw(std::string &value, std::endian endian) override;

            private:
                bool m_hasInvalidValue = false;
            };

        }

        /**
         * @brief Adds a new entry to the data inspector
         * @param unlocalizedName The unlocalized name of the entry
         * @param requiredSize The minimum required number of bytes available for the entry to appear
         * @param displayGeneratorFunction The function that will be called to generate the display function
         * @param editingFunction The function that will be called to edit the data
         */
        void add(
            const UnlocalizedString &unlocalizedName,
            size_t requiredSize,
            impl::GeneratorFunction displayGeneratorFunction,
            std::optional<impl::EditingFunction> editingFunction = std::nullopt
        );

        /**
         * @brief Adds a new entry to the data inspector
         * @param unlocalizedName The unlocalized name of the entry
         * @param requiredSize The minimum required number of bytes available for the entry to appear
         * @param maxSize The maximum number of bytes to read from the data
         * @param displayGeneratorFunction The function that will be called to generate the display function
         * @param editingFunction The function that will be called to edit the data
         * @param sizeFunction For an entry whose requiredSize and maxSize differ, reports how many of
         * the read bytes it actually used. Clicking the row then selects that many bytes.
         */
        void add(
            const UnlocalizedString &unlocalizedName,
            size_t requiredSize,
            size_t maxSize,
            impl::GeneratorFunction displayGeneratorFunction,
            std::optional<impl::EditingFunction> editingFunction = std::nullopt,
            std::optional<impl::SizeFunction> sizeFunction = std::nullopt
        );

        /**
         * @brief Allows adding new menu items to data inspector row context menus. Call this function inside the
         * draw function of the data inspector row definition.
         * @param function Callback that will draw menu items
         */
        void drawMenuItems(const std::function<void()> &function);

    }

}