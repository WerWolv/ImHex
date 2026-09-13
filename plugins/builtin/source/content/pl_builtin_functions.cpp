#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/content_registry/pattern_language.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/encoding_file.hpp>
#include <hex/trace/stacktrace.hpp>

#include <pl/core/token.hpp>
#include <pl/core/evaluator.hpp>

#include <pl/patterns/pattern.hpp>

namespace hex::plugin::builtin {

    namespace {

        /**
         * @brief Resolves a bundled table's name, or raw table content, to a table
         *
         * getEncodingByName() caches a bundled table for the life of the process. Raw content
         * is parsed into `storage`, which the caller owns, so a script that passes a varying
         * inline table in a loop cannot grow that cache without bound.
         */
        const EncodingFile& resolveEncoding(const std::string &encoding, std::optional<EncodingFile> &storage) {
            if (const auto *knownEncoding = getEncodingByName(encoding); knownEncoding != nullptr)
                return *knownEncoding;

            return storage.emplace(EncodingFile::Type::Thingy, encoding);
        }

    }

    void registerPatternLanguageFunctions() {
        using namespace pl::core;
        using FunctionParameterCount = pl::api::FunctionParameterCount;

        {
            const pl::api::Namespace nsHexCore = { "builtin", "hex", "core" };

            /* get_selection() */
            ContentRegistry::PatternLanguage::addFunction(nsHexCore, "get_selection", FunctionParameterCount::none(), [](Evaluator *, auto) -> std::optional<Token::Literal> {
                if (!ImHexApi::HexEditor::isSelectionValid())
                    return std::numeric_limits<u128>::max();

                auto selection = ImHexApi::HexEditor::getSelection();

                return (u128(selection->getStartAddress()) << 64 | u128(selection->getSize()));
            });

            /* add_virtual_file(path, pattern) */
            ContentRegistry::PatternLanguage::addFunction(nsHexCore, "add_virtual_file", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                auto path = params[0].toString(false);
                auto pattern = params[1].toPattern();

                Region region = Region::Invalid();
                if (pattern->getSection() == pl::ptrn::Pattern::MainSectionId)
                    region = Region(pattern->getOffset(), pattern->getSize());

                ImHexApi::HexEditor::addVirtualFile(path, pattern->getBytes(), region);

                return std::nullopt;
            });
        }

        {
            const pl::api::Namespace nsHexPrv = { "builtin", "hex", "prv" };

            /* get_information() */
            ContentRegistry::PatternLanguage::addFunction(nsHexPrv, "get_information", FunctionParameterCount::between(1, 2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                std::string category = params[0].toString(false);
                std::string argument = params.size() == 2 ? params[1].toString(false) : "";

                if (!ImHexApi::Provider::isValid())
                    return u128(0);

                auto provider = ImHexApi::Provider::get();
                if (!provider->isAvailable())
                    return u128(0);

                return std::visit(
                    [](auto &&value) -> Token::Literal {
                        return value;
                    },
                    provider->queryInformation(category, argument)
                );
            });
        }

        {
            const pl::api::Namespace nsHexDec = { "builtin", "hex", "dec" };

            /* demangle(mangled_string) */
            ContentRegistry::PatternLanguage::addFunction(nsHexDec, "demangle", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto mangledString = params[0].toString(false);

                return trace::demangle(mangledString);
            });

            /* decode(bytes, encoding) */
            ContentRegistry::PatternLanguage::addFunction(nsHexDec, "decode", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto bytes = params[0].toBytes();
                const auto encoding = params[1].toString(false);

                std::optional<EncodingFile> storage;
                const auto &encodingFile = resolveEncoding(encoding, storage);

                // decodeAll() alone would show "." for an unmapped byte, not report it.
                if (!encodingFile.isFullyMapped(bytes))
                    err::E0012.throwError("The given bytes contain a sequence that has no representation in this encoding.");

                return encodingFile.decodeAll(bytes);
            });

            /* encode(string, encoding) */
            ContentRegistry::PatternLanguage::addFunction(nsHexDec, "encode", FunctionParameterCount::exactly(2), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto string = params[0].toString(false);
                const auto encoding = params[1].toString(false);

                std::optional<EncodingFile> storage;
                const auto &encodingFile = resolveEncoding(encoding, storage);

                if (!encodingFile.canEncode())
                    err::E0012.throwError("This encoding is ambiguous (multiple byte sequences decode to the same value, or one decoded value is a prefix of another) and can therefore not be used to encode data.");

                auto bytes = encodingFile.encodeAll(string);
                if (!bytes.has_value())
                    err::E0012.throwError(fmt::format("The string '{}' contains a character sequence that has no representation in this encoding.", string));

                return std::string(bytes->begin(), bytes->end());
            });
        }

        {
            const pl::api::Namespace nsHexHttp = { "builtin", "hex", "http" };

            /* get(url) */
            ContentRegistry::PatternLanguage::addDangerousFunction(nsHexHttp, "get", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto url = params[0].toString(false);

                hex::HttpRequest request("GET", url);
                return request.execute().get().getData();
            });
        }
    }
}
