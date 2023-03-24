#include <hex/api/content_registry.hpp>

#include <hex/api/localization.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>

#include <content/helpers/math_evaluator.hpp>

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

        ContentRegistry::CommandPaletteCommands::addHandler(
                ContentRegistry::CommandPaletteCommands::Type::SymbolCommand,
                ">",
                [](const auto &input) {
                    std::vector<ContentRegistry::CommandPaletteCommands::impl::QueryResult> result;

                    for (const auto &[priority, entry] : ContentRegistry::Interface::impl::getMenuItems()) {
                        if (!entry.enabledCallback())
                            continue;

                        std::vector<std::string> names;
                        std::transform(entry.unlocalizedNames.begin(), entry.unlocalizedNames.end(), std::back_inserter(names), [](auto &name) { return LangEntry(name); });

                        if (auto combined = wolv::util::combineStrings(names, " -> "); hex::containsIgnoreCase(combined, input) && !combined.contains(ContentRegistry::Interface::impl::SeparatorValue) && !combined.contains(ContentRegistry::Interface::impl::SubMenuValue)) {
                            result.emplace_back(ContentRegistry::CommandPaletteCommands::impl::QueryResult {
                                std::move(combined),
                                [entry](const auto&) { entry.callback(); }
                            });
                        }
                    }

                    return result;
                },
                [](auto input) {
                    return hex::format("Menu Item: {}", input.data());
                });

    }

}