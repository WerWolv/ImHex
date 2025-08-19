#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

EXPORT_MODULE namespace hex {

    /* Data Inspector Registry. Allows adding of new types to the data inspector */
    namespace ContentRegistry::DataInspector {

        enum class NumberDisplayStyle : u8 {
            Decimal,
            Hexadecimal,
            Octal
        };

        namespace impl {

            using DisplayFunction   = std::function<std::string()>;
            using EditingFunction   = std::function<std::vector<u8>(std::string, std::endian)>;
            using GeneratorFunction = std::function<DisplayFunction(const std::vector<u8> &, std::endian, NumberDisplayStyle)>;

            struct Entry {
                UnlocalizedString unlocalizedName;
                size_t requiredSize;
                size_t maxSize;
                GeneratorFunction generatorFunction;
                std::optional<EditingFunction> editingFunction;
            };

            const std::vector<Entry>& getEntries();

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
         */
        void add(
            const UnlocalizedString &unlocalizedName,
            size_t requiredSize,
            size_t maxSize,
            impl::GeneratorFunction displayGeneratorFunction,
            std::optional<impl::EditingFunction> editingFunction = std::nullopt
        );

        /**
         * @brief Allows adding new menu items to data inspector row context menus. Call this function inside the
         * draw function of the data inspector row definition.
         * @param function Callback that will draw menu items
         */
        void drawMenuItems(const std::function<void()> &function);

    }

}