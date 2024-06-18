#pragma once

#include "export_formatter.hpp"

namespace hex::plugin::builtin::export_fmt {

    class ExportFormatterCsv : public ExportFormatter {
    public:
        ExportFormatterCsv() : ExportFormatter("csv") {}

        explicit ExportFormatterCsv(std::string name) : ExportFormatter(std::move(name)) {}

        ~ExportFormatterCsv() override = default;

        [[nodiscard]] std::vector<u8> format(const std::vector<Occurrence> &occurrences, std::function<std::string(Occurrence)> occurrenceFunc) override {
            char separator = getSeparatorCharacter();

            std::string result;
            result += fmt::format("offset{}size{}data\n", separator, separator);


            for (const auto &occurrence : occurrences) {
                std::string formattedResult = occurrenceFunc(occurrence);
                std::string escapedResult;
                escapedResult.reserve(formattedResult.size() * 2);

                for (char ch : formattedResult) {
                    if (ch == '"') {
                        escapedResult += "\"\"";
                    } else if (ch == '\n' || ch == '\r') {
                        escapedResult += ' '; // Replace newlines with spaces
                    } else {
                        escapedResult += ch;
                    }
                }

                bool needsQuotes = escapedResult.find(separator) != std::string::npos || escapedResult.find('"') != std::string::npos;
                if (needsQuotes) {
                    escapedResult.insert(0, 1, '"');
                    escapedResult.push_back('"');
                }

                const auto line = fmt::format("0x{:08X}{}0x{}{}{}",
                                              occurrence.region.getStartAddress(),
                                              separator,
                                              occurrence.region.getSize(),
                                              separator,
                                              escapedResult);
                result += line;
                result += '\n';
            }

            return { result.begin(), result.end() };
        }

    protected:
        [[nodiscard]] virtual char getSeparatorCharacter() const { return ','; }
    };

}