#include <hex/api/content_registry.hpp>

#include <hex/helpers/net.hpp>

#include <pl/core/token.hpp>
#include <pl/core/log_console.hpp>
#include <pl/core/evaluator.hpp>
#include <pl/patterns/pattern.hpp>

namespace hex::plugin::builtin {

    void registerPatternLanguageFunctions() {
        using namespace pl::core;
        using FunctionParameterCount = pl::api::FunctionParameterCount;

        pl::api::Namespace nsHexCore = { "builtin", "hex", "core" };
        {
            /* get_selection() */
            ContentRegistry::PatternLanguage::addDangerousFunction(nsHexCore, "get_selection", FunctionParameterCount::none(), [](Evaluator *, auto) -> std::optional<Token::Literal> {
                if (!ImHexApi::HexEditor::isSelectionValid())
                    return std::numeric_limits<u128>::max();

                auto selection = ImHexApi::HexEditor::getSelection();

                return u128(u128(selection->getStartAddress()) << 64 | u128(selection->getSize()));
            });
        }

        pl::api::Namespace nsHexHttp = { "builtin", "hex", "http" };
        {
            /* get(url) */
            ContentRegistry::PatternLanguage::addDangerousFunction(nsHexHttp, "get", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto url = Token::literalToString(params[0], false);

                hex::Net net;
                return net.getString(url).get().body;
            });
        }
    }
}
