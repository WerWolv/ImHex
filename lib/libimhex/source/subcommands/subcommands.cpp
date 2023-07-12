#include<iostream>
#include<numeric>
#include<string_view>
#include<ranges>
#include<stdlib.h>

#include "hex/subcommands/subcommands.hpp"

#include <hex/api/event.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/helpers/logger.hpp>

namespace hex::subcommands {

    std::optional<SubCommand> findSubCommand(const std::string &arg) {
        for (auto &plugin : PluginManager::getPlugins()) {
            for (auto &subCommand : plugin.getSubCommands()) {
                if (hex::format("--{}", subCommand.commandKey) == arg) {
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
        if (args.empty()) return;

        std::vector<std::pair<SubCommand, std::vector<std::string>>> subCommands;

        auto argsIter = args.begin();

        // get subcommand associated with the first argument
        std::optional<SubCommand> currentSubCommand = findSubCommand(*argsIter);

        if (currentSubCommand) {
            argsIter++;
            // if it is a valid subcommand, remove it from the argument list
        } else {
            // if no (valid) subcommand was provided, the default one is --open
            currentSubCommand = findSubCommand("--open");
        }

        // arguments of the current subcommand
        std::vector<std::string> currentSubCommandArgs;

        // compute all subcommands to run
        while (argsIter != args.end()) {
            const std::string &arg = *argsIter;

            if (arg == "--othercmd") {
                // save command to run
                if (currentSubCommand) {
                    subCommands.push_back({*currentSubCommand, currentSubCommandArgs});
                }

                currentSubCommand = std::nullopt;
                currentSubCommandArgs = { };

            } else if (currentSubCommand) {
                // add current argument to the current command
                currentSubCommandArgs.push_back(arg);
            } else { 
                // get next subcommand from current argument
                currentSubCommand = findSubCommand(arg);
                if (!currentSubCommand) {
                    log::error("No subcommand named '{}' found", arg);
                    exit(EXIT_FAILURE);
                }
            }

            argsIter++;
        }

        // save last command to run
        if (currentSubCommand) {
            subCommands.push_back({*currentSubCommand, currentSubCommandArgs});
        }

        // run the subcommands
        for (auto& subCommandPair : subCommands) {
            subCommandPair.first.callback(subCommandPair.second);
        }

        // exit the process if its not the main instance (the commands have been forwarded to another instance)
        bool isMainInstance;
        EventManager::post<IsMainInstance>(isMainInstance);
        if (!isMainInstance) {
            exit(0);
        }
    }

    void forwardSubCommand(const std::string &cmdName, const std::vector<std::string> &args) {
        log::debug("Forwarding subcommand {} (maybe to us)", cmdName);
        std::string dataStr = std::accumulate(args.begin(), args.end(), std::string("\0"));

        std::vector<u8> data(dataStr.begin(), dataStr.end());
        
        EventManager::post<SendMessageToMainInstance>(hex::format("command/{}", cmdName), data);
    }

    void registerSubCommand(const std::string &cmdName, const ForwardCommandHandler &handler) {
        log::debug("Registered new forward command handler: {}", cmdName);

        ImHexApi::Messaging::impl::getHandlers().insert({ hex::format("command/{}", cmdName), [handler](const std::vector<u8> &eventData){
            std::string str((const char*) eventData.data(), eventData.size());

            std::vector<std::string> args;

            for (const auto &arg_view : std::views::split(str, '\0')) {
                std::string arg(arg_view.data(), arg_view.size());
                args.push_back(arg);
            }

            handler(args);
        }});
    }
}
