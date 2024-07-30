#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>

#include <hex/api/content_registry.hpp>

namespace hex::plugin::builtin::export_fmt {

    using Occurrence = hex::ContentRegistry::DataFormatter::impl::FindOccurrence;

    /**
     * Base class for creating export formatters for different file formats, used in the Results section of the 'Find' view
     */
    class ExportFormatter {
    public:
        explicit ExportFormatter(std::string name) : mName(std::move(name)) {}

        virtual ~ExportFormatter() = default;

        [[nodiscard]] const std::string &getName() const {
            return this->mName;
        }

        /**
         * Main export formatter function
         * @param occurrences A list of search occurrences found by the 'Find' view
         * @param occurrenceFunc A string formatter function used to transform each occurrence to a string form. May be ignored for custom/binary exporter formats
         * @return An array of bytes representing the exported data to be written into the target file
         */
        [[nodiscard]] virtual std::vector<u8> format(const std::vector<Occurrence> &occurrences, std::function<std::string(Occurrence)> occurrenceFunc) = 0;

    private:
        std::string mName;
    };

}