#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>

#include <hex/api/content_registry.hpp>

namespace hex::plugin::builtin::export_fmt {

    using Occurrence = hex::ContentRegistry::DataFormatter::impl::FindOccurrence;

    class ExportFormatter {
    public:
        explicit ExportFormatter(std::string name) : mName(std::move(name)) {}

        virtual ~ExportFormatter() = default;

        [[nodiscard]] const std::string &getName() const {
            return this->mName;
        }

        [[nodiscard]] virtual std::string getFileExtension() const = 0;
        [[nodiscard]] virtual std::vector<u8> format(const std::vector<Occurrence> &occurrences, std::function<std::string(Occurrence)> occurrenceFunc) = 0;

    private:
        std::string mName;
    };

}