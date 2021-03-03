#include <hex/plugin.hpp>

#include "math_evaluator.hpp"

namespace hex::plugin::builtin {

    void registerCommandPaletteCommands() {

        hex::ContentRegistry::CommandPaletteCommands::add(
                hex::ContentRegistry::CommandPaletteCommands::Type::SymbolCommand,
                "#", "hex.builtin.command.calc.desc",
                [](auto input) {
                    hex::MathEvaluator evaluator;
                    evaluator.registerStandardVariables();
                    evaluator.registerStandardFunctions();

                    std::optional<long double> result;

                    try {
                        result = evaluator.evaluate(input);
                    } catch (std::exception &e) {}


                    if (result.has_value())
                        return hex::format("#{0} = %{1}", input.data(), result.value());
                    else
                        return hex::format("#{0} = ???", input.data());
                });

        hex::ContentRegistry::CommandPaletteCommands::add(
                hex::ContentRegistry::CommandPaletteCommands::Type::KeywordCommand,
                "/web", "hex.builtin.command.web.desc",
                [](auto input) {
                    return hex::format("hex.builtin.command.web.result"_lang, input.data());
                },
                [](auto input) {
                    hex::openWebpage(input);
                });

    }

}