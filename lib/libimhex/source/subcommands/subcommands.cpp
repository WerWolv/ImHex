#include <numeric>
#include <string_view>
#include <ranges>

#include "hex/subcommands/subcommands.hpp"

#include <hex/api/events/requests_lifecycle.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/api/imhex_api/messaging.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/fmt.hpp>

namespace hex::subcommands {

    std::optional<SubCommand> findSubCommand(const std::string &arg) {
        if (arg == "-" || arg == "--")
            return std::nullopt;

        for (auto &plugin : PluginManager::getPlugins()) {
            for (auto &subCommand : plugin.getSubCommands()) {
                if (fmt::format("--{}", subCommand.commandLong) == arg || fmt::format("-{}", subCommand.commandShort) == arg) {
                    return subCommand;
                }
            }
        }

        return std::nullopt;
    }

    void processArguments(const std::vector<std::string> &args) {
        // If no arguments, do not even try to process arguments
        // (important because this function will exit ImHex if an instance is already opened,
        // and we don't want that if no arguments were provided)
        if (args.empty())
            return;

        std::vector<std::pair<SubCommand, std::vector<std::string>>> subCommands;

        auto argsIter = args.begin();

        std::optional<SubCommand> currentSubCommand;
        std::vector<std::string> currentSubCommandArgs;

        if (*argsIter == "--") {
            // Treat the rest of the args as files
            ++argsIter;
            std::vector<std::string> remainingArgs(argsIter, args.end());
            subCommands.emplace_back(*findSubCommand("--open"), remainingArgs);
            argsIter = args.end(); // Skip while loop
        } else if (!findSubCommand(*argsIter)) {
            // First argument not a subcommand, treat all args as files
            subCommands.emplace_back(*findSubCommand("--open"), args);
            argsIter = args.end(); // Skip while loop
        }

        while (argsIter != args.end()) {
            // Get subcommand associated with the argument
            // Guaranteed to find a match on the first argument, as the other case has been handled above
            const auto newSubCommand = findSubCommand(*argsIter);
            if (*argsIter == "--") {
                // Treat the rest of the args as files
                subCommands.emplace_back(*currentSubCommand, currentSubCommandArgs);

                ++argsIter;
                std::vector<std::string> remainingArgs(argsIter, args.end());

                subCommands.emplace_back(*findSubCommand("--open"), remainingArgs);

                currentSubCommand = std::nullopt;
                currentSubCommandArgs.clear();
                break;
            }

            // Will always take this `if` statement on the first time through the loop
            if (newSubCommand.has_value()) {
                if (currentSubCommand.has_value()) {
                    subCommands.emplace_back(*currentSubCommand, currentSubCommandArgs);
                }

                if (newSubCommand->type == SubCommand::Type::SubCommand) {
                    ++argsIter;
                    std::vector<std::string> remainingArgs(argsIter, args.end());
                    subCommands.emplace_back(*newSubCommand, remainingArgs);

                    currentSubCommand = std::nullopt;
                    currentSubCommandArgs.clear();
                    break;
                }

                currentSubCommand = newSubCommand;
                currentSubCommandArgs.clear();
            } else {
                currentSubCommandArgs.push_back(*argsIter);
            }

            ++argsIter;
        }

        // Save last command to run
        if (currentSubCommand.has_value()) {
            subCommands.emplace_back(*currentSubCommand, currentSubCommandArgs);
        }

        // Run the subcommands
        for (const auto &[subcommand, subCommandArgs] : subCommands) {
            subcommand.callback(subCommandArgs);
        }

        // Exit the process if it's not the main instance (the commands have been forwarded to another instance)
        if (!ImHexApi::System::isMainInstance()) {
            std::exit(0);
        }
    }

    void forwardSubCommand(const std::string &cmdName, const std::vector<std::string> &args) {
        log::debug("Forwarding subcommand {} (maybe to us)", cmdName);

        std::vector<u8> data;
        if (!args.empty()) {
            for (const auto &arg: args) {
                data.insert(data.end(), arg.begin(), arg.end());
                data.push_back('\0');
            }

            data.pop_back();
        }

        SendMessageToMainInstance::post(fmt::format("command/{}", cmdName), data);
    }

    void registerSubCommand(const std::string &cmdName, const ForwardCommandHandler &handler) {
        log::debug("Registered new forward command handler: {}", cmdName);

        ImHexApi::Messaging::registerHandler(fmt::format("command/{}", cmdName), [handler](const std::vector<u8> &eventData){
            std::string string(reinterpret_cast<const char *>(eventData.data()), eventData.size());

            std::vector<std::string> args;

            for (const auto &argument : std::views::split(string, char(0x00))) {
                std::string arg(argument.data(), argument.size());
                args.push_back(arg);
            }

            handler(args);
        });
    }
}
