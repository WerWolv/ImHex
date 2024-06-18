#pragma once

#include "export_formatter.hpp"

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin::export_fmt {

    class ExportFormatterJson : public ExportFormatter {
    public:
        ExportFormatterJson() : ExportFormatter("json") {}

        ~ExportFormatterJson() override = default;

        std::vector<u8> format(const std::vector<Occurrence> &occurrences, std::function<std::string (Occurrence)> occurrenceFunc) override {
            nlohmann::json resultJson;

            for (const auto &occurrence : occurrences) {
                std::string formattedResult = occurrenceFunc(occurrence);

                nlohmann::json obj = {
                        { "offset", occurrence.region.getStartAddress() },
                        { "size", occurrence.region.getSize() },
                        { "data", formattedResult }
                };

                resultJson.push_back(obj);
            }

            auto result = resultJson.dump(4);
            return { result.begin(), result.end() };
        }
    };
}