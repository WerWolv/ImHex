#include <hex.hpp>

#include <hex/api/content_registry.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

#include <archive.h>
#include <archive_entry.h>

#include <wolv/utils/guards.hpp>

#include <vector>
#include <optional>

namespace hex::plugin::decompress {

    void registerPatternLanguageFunctions() {
        using namespace pl::core;
        using FunctionParameterCount = pl::api::FunctionParameterCount;

        const pl::api::Namespace nsHexDec = { "builtin", "hex", "dec" };

        /* decompress() */
        ContentRegistry::PatternLanguage::addFunction(nsHexDec, "decompress", FunctionParameterCount::exactly(2), [](Evaluator *evaluator, auto params) -> std::optional<Token::Literal> {
            const auto inputPattern = params[0].toPattern();
            auto &section = evaluator->getSection(params[1].toUnsigned());

            std::vector<u8> compressedData;
            compressedData.resize(inputPattern->getSize());
            evaluator->readData(inputPattern->getOffset(), compressedData.data(), compressedData.size(), inputPattern->getSection());

            auto inArchive = archive_read_new();
            ON_SCOPE_EXIT {
                archive_read_close(inArchive);
                archive_read_free(inArchive);
            };

            archive_read_support_filter_all(inArchive);
            archive_read_support_format_raw(inArchive);

            archive_read_open_memory(inArchive, compressedData.data(), compressedData.size());

            archive_entry *entry = nullptr;
            while (archive_read_next_header(inArchive, &entry) == ARCHIVE_OK) {
                const void *block = nullptr;
                size_t size = 0x00;
                i64 offset = 0x00;

                while (archive_read_data_block(inArchive, &block, &size, &offset) == ARCHIVE_OK) {
                    section.resize(section.size() + size);
                    std::memcpy(section.data(), block, size);
                }
            }

            return std::nullopt;
        });
    }

}