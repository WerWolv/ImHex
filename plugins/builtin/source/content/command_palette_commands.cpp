#include <hex/api/content_registry.hpp>

#include <hex/api/localization.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>

#include "math_evaluator.hpp"

namespace hex::plugin::builtin {

    void registerCommandPaletteCommands() {

        ContentRegistry::CommandPaletteCommands::add(
            ContentRegistry::CommandPaletteCommands::Type::SymbolCommand,
            "#",
            "hex.builtin.command.calc.desc",
            [](auto input) {
                hex::MathEvaluator<long double> evaluator;
                evaluator.registerStandardVariables();
                evaluator.registerStandardFunctions();

                std::optional<long double> result;

                result = evaluator.evaluate(input);
                if (result.has_value())
                    return hex::format("#{0} = {1}", input.data(), result.value());
                else if (evaluator.hasError())
                    return hex::format("Error: {}", *evaluator.getLastError());
                else
                    return std::string("???");
            });

        ContentRegistry::CommandPaletteCommands::add(
            ContentRegistry::CommandPaletteCommands::Type::KeywordCommand,
            "/web",
            "hex.builtin.command.web.desc",
            [](auto input) {
                return hex::format("hex.builtin.command.web.result"_lang, input.data());
            },
            [](auto input) {
                hex::openWebpage(input);
            });

        ContentRegistry::CommandPaletteCommands::add(
            ContentRegistry::CommandPaletteCommands::Type::SymbolCommand,
            "$",
            "hex.builtin.command.cmd.desc",
            [](auto input) {
                return hex::format("hex.builtin.command.cmd.result"_lang, input.data());
            },
            [](auto input) {
                hex::runCommand(input);
            });
    }

}