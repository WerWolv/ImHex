#pragma once

#include "export_formatter.hpp"

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin::export_fmt {

    using json = nlohmann::json;

    class ExportFormatterJson : public ExportFormatter {
    public:
        ExportFormatterJson() : ExportFormatter("json") {}

        ~ExportFormatterJson() override = default;

        std::vector<u8> format(const std::vector<Occurrence> &occurrences, std::function<std::string (Occurrence)> occurrenceFunc) override {
            json results_array;

            for (const auto &occurrence : occurrences) {
                std::string formattedResult = occurrenceFunc(occurrence);

                json obj = {
                        { "offset", occurrence.region.getStartAddress() },
                        { "size", occurrence.region.getSize() },
                        { "data", formattedResult }
                };
                results_array.push_back(obj);
            }

            const auto& result_string = results_array.dump(4);
            return { result_string.begin(), result_string.end() };
        }
    };
}