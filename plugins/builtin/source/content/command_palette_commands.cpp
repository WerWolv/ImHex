#include <hex/plugin.hpp>

#include "math_evaluator.hpp"

namespace hex::plugin::builtin {

    void registerCommandPaletteCommands() {

        hex::ContentRegistry::CommandPaletteCommands::add(
                hex::ContentRegistry::CommandPaletteCommands::Type::SymbolCommand,
                "#", "Calculator",
                [](auto input) {
                    hex::MathEvaluator evaluator;
                    evaluator.registerStandardVariables();
                    evaluator.registerStandardFunctions();

                    std::optional<long double> result;

                    try {
                        result = evaluator.evaluate(input);
                    } catch (std::exception &e) {}


                    if (result.has_value())
                        return hex::format("#%s = %Lf", input.data(), result.value());
                    else
                        return hex::format("#%s = ???", input.data());
                });

        hex::ContentRegistry::CommandPaletteCommands::add(
                hex::ContentRegistry::CommandPaletteCommands::Type::KeywordCommand,
                "/web", "Website lookup",
                [](auto input) {
                    return hex::format("Navigate to '%s'", input.data());
                },
                [](auto input) {
                    hex::openWebpage(input);
                });

    }

}