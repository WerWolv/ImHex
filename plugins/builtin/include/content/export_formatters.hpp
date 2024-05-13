#pragma once

#include "export_formatters/export_formatter.hpp"
#include "export_formatters/export_formatter_csv.hpp"
#include "export_formatters/export_formatter_tsv.hpp"

namespace hex::plugin::builtin::export_fmt {

    using ExportFormatters = std::tuple<
            ExportFormatterCsv,
            ExportFormatterTsv
    >;

    using ExportFormatterArray = std::array<std::unique_ptr<hex::plugin::builtin::export_fmt::ExportFormatter>, std::tuple_size_v<ExportFormatters>>;

    template<size_t N = 0>
    auto createFormatters(ExportFormatterArray &&result = {}) {
        auto formatter = std::unique_ptr<hex::plugin::builtin::export_fmt::ExportFormatter>(new typename std::tuple_element<N, ExportFormatters>::type());

        result[N] = std::move(formatter);

        if constexpr (N + 1 < std::tuple_size_v<ExportFormatters>) {
            return createFormatters<N + 1>(std::move(result));
        } else {
            return result;
        }
    }
}