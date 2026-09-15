#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>

#include <string>
#include <functional>
#include <vector>
#include <variant>

EXPORT_MODULE namespace hex {

    #if !defined(HEX_MODULE_EXPORT)
        namespace prv { class Provider; }
    #endif

    /* Data Formatter Registry. Allows adding formatters that are used in the Copy-As menu for example */
    namespace ContentRegistry::DataFormatter {

        /**
         * @brief A simple table of string/number cells used by export formatters
         */
        class ExportTable {
        public:
            using Cell = std::variant<std::string, u64, i64, double>;

            /**
             * @brief Creates a table with the given column headers
             * @throws std::invalid_argument if headers are not unique
             */
            explicit ExportTable(std::vector<std::string> headers);

            /**
             * @brief Appends a row to the table
             * @throws std::invalid_argument if the row size does not match the header count
             */
            void addRow(std::vector<Cell> row);

            [[nodiscard]] const std::vector<std::string>& getHeaders() const { return m_headers; }
            [[nodiscard]] const std::vector<std::vector<Cell>>& getRows() const { return m_rows; }
        private:
            std::vector<std::string> m_headers;
            std::vector<std::vector<Cell>> m_rows;
        };

        namespace impl {

            using ExportMenuCallback = std::function<std::string(prv::Provider *provider, u64 address, size_t size, bool preview)>;
            struct ExportMenuEntry {
                UnlocalizedString unlocalizedName;
                ExportMenuCallback callback;
            };

            /**
             * @brief Converts an ExportTable into the raw bytes of an exported file
             */
            using ExportFormatterCallback = std::function<std::vector<u8>(const ExportTable& table)>;

            struct ExportFormatterEntry {
                UnlocalizedString unlocalizedName;
                std::string fileExtension;
                ExportFormatterCallback callback;
            };

            /**
             * @brief Retrieves a list of all registered data formatters used by the 'File -> Export' menu
             */
            const std::vector<ExportMenuEntry>& getExportMenuEntries();

            /**
             * @brief Retrieves a list of all registered general export formatters for table data
             */
            const std::vector<ExportFormatterEntry>& getExportFormatterEntries();
        }


        /**
         * @brief Adds a new data formatter used by the 'File -> Export' menu
         * @param unlocalizedName The unlocalized name of the formatter
         * @param callback The function to call to format the data
         */
        void addExportMenuEntry(const UnlocalizedString &unlocalizedName, const impl::ExportMenuCallback &callback);

        /**
         * @brief Adds a new general data exporter for table formats
         * @param unlocalizedName The unlocalized name of the formatter
         * @param fileExtension The file extension to use for the exported file
         * @param callback The function to call to format the data
         */
        void addExportFormatter(const UnlocalizedString &unlocalizedName, const std::string &fileExtension, const impl::ExportFormatterCallback &callback);

    }

}
