#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/http_requests.hpp>

#include <pl/core/token.hpp>
#include <pl/core/evaluator.hpp>

#include <content/helpers/demangle.hpp>
#include <pl/patterns/pattern.hpp>

namespace hex::plugin::builtin {

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

                return u128(u128(selection->getStartAddress()) << 64 | u128(selection->getSize()));
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

                return hex::plugin::builtin::demangle(mangledString);
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
