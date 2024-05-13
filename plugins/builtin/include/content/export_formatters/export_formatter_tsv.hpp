#pragma once

#include "export_formatter_csv.hpp"

namespace hex::plugin::builtin::export_fmt {

    class ExportFormatterTsv : public ExportFormatterCsv {
    public:
        ExportFormatterTsv() : ExportFormatterCsv("tsv") {}

        ~ExportFormatterTsv() override = default;

        [[nodiscard]] std::string getFileExtension() const override { return "tsv"; }

    protected:
        [[nodiscard]] char getSeparatorCharacter() const override { return '\t'; }
    };

}