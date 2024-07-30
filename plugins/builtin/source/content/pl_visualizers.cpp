#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>

#include <pl/core/evaluator.hpp>

namespace hex::plugin::builtin {

    void drawHexVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments);
    void drawChunkBasedEntropyVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments);

    void registerPatternLanguageVisualizers() {
        using ParamCount = pl::api::FunctionParameterCount;

        ContentRegistry::PatternLanguage::addVisualizer("hex_viewer", drawHexVisualizer, ParamCount::exactly(1));
        ContentRegistry::PatternLanguage::addVisualizer("chunk_entropy",    drawChunkBasedEntropyVisualizer,    ParamCount::exactly(2));
    }

}