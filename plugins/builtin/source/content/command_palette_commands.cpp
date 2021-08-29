#include <hex/api/content_registry.hpp>

#include "math_evaluator.hpp"

namespace hex::plugin::builtin {

    void registerCommandPaletteCommands() {

        ContentRegistry::CommandPaletteCommands::add(
                ContentRegistry::CommandPaletteCommands::Type::SymbolCommand,
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

        ContentRegistry::CommandPaletteCommands::add(
                ContentRegistry::CommandPaletteCommands::Type::KeywordCommand,
                "/web", "hex.builtin.command.web.desc",
                [](auto input) {
                    return hex::format("hex.builtin.command.web.result"_lang, input.data());
                },
                [](auto input) {
                    hex::openWebpage(input);
                });

        ContentRegistry::CommandPaletteCommands::add(
                ContentRegistry::CommandPaletteCommands::Type::SymbolCommand,
                "$", "hex.builtin.command.cmd.desc",
                [](auto input) {
                    return hex::format("hex.builtin.command.cmd.result"_lang, input.data());
                },
                [](auto input) {
                    hex::runCommand(input);
                });

    }

}