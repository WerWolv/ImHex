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

        pl::api::Namespace nsStdHttp = { "builtin", "std", "http" };
        {
            /* get(url) */
            ContentRegistry::PatternLanguage::addDangerousFunction(nsStdHttp, "get", FunctionParameterCount::exactly(1), [](Evaluator *, auto params) -> std::optional<Token::Literal> {
                const auto url = Token::literalToString(params[0], false);

                hex::Net net;
                return net.getString(url).get().body;
            });
        }
    }
}
