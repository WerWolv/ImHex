#pragma once

#include <hex.hpp>

#include <hex/api/localization_manager.hpp>

#include <string>
#include <functional>
#include <vector>

EXPORT_MODULE namespace hex {

    #if !defined(HEX_MODULE_EXPORT)
        namespace prv { class Provider; }
    #endif

    /* Data Formatter Registry. Allows adding formatters that are used in the Copy-As menu for example */
    namespace ContentRegistry::DataFormatter {

        namespace impl {

            using Callback = std::function<std::string(prv::Provider *provider, u64 address, size_t size, bool preview)>;
            struct ExportMenuEntry {
                UnlocalizedString unlocalizedName;
                Callback callback;
            };

            struct FindOccurrence {
                Region region;
                std::endian endian = std::endian::native;
                enum class DecodeType : u8 { ASCII, UTF8, Binary, UTF16, Unsigned, Signed, Float, Double } decodeType;
                bool selected;
                std::string string;
            };

            using FindExporterCallback = std::function<std::vector<u8>(const std::vector<FindOccurrence>&, std::function<std::string(FindOccurrence)>)>;
            struct FindExporterEntry {
                UnlocalizedString unlocalizedName;
                std::string fileExtension;
                FindExporterCallback callback;
            };

            /**
             * @brief Retrieves a list of all registered data formatters used by the 'File -> Export' menu
             */
            const std::vector<ExportMenuEntry>& getExportMenuEntries();

            /**
             * @brief Retrieves a list of all registered data formatters used in the Results section of the 'Find' view
             */
            const std::vector<FindExporterEntry>& getFindExporterEntries();

        }


        /**
         * @brief Adds a new data formatter
         * @param unlocalizedName The unlocalized name of the formatter
         * @param callback The function to call to format the data
         */
        void addExportMenuEntry(const UnlocalizedString &unlocalizedName, const impl::Callback &callback);

        /**
         * @brief Adds a new data exporter for Find results
         * @param unlocalizedName The unlocalized name of the formatter
         * @param fileExtension The file extension to use for the exported file
         * @param callback The function to call to format the data
         */
        void addFindExportFormatter(const UnlocalizedString &unlocalizedName, const std::string &fileExtension, const impl::FindExporterCallback &callback);

    }

}