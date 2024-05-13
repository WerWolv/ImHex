#pragma once

#include "export_formatter.hpp"

#include <sstream>

namespace hex::plugin::builtin::export_fmt {

    class ExportFormatterCsv : public ExportFormatter {
    public:
        ExportFormatterCsv() : ExportFormatter("csv") {}

        explicit ExportFormatterCsv(std::string name) : ExportFormatter(name) {}

        ~ExportFormatterCsv() override = default;

        [[nodiscard]] std::string getFileExtension() const override { return "csv"; }

        [[nodiscard]] std::vector<u8> format(const PerProvider<std::vector<Occurrence>> &occurrences, prv::Provider *provider, std::function<std::string(Occurrence)> occurrenceFunc) override {
            char separator = getSeparatorCharacter();

            std::ostringstream ss;
            ss << fmt::format("offset{}size{}data", separator, separator) << std::endl;


            for (const auto &occurrence : occurrences.get(provider)) {
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

                const auto line = fmt::format("0x{:08X}{}0x{:X}{}{}",
                                              occurrence.region.getStartAddress(),
                                              separator,
                                              occurrence.region.getSize(),
                                              separator,
                                              escapedResult);
                ss << line << std::endl;
            }

            auto result = ss.str();
            return { result.begin(), result.end() };
        }

    protected:
        [[nodiscard]] virtual char getSeparatorCharacter() const { return ','; }
    };

}