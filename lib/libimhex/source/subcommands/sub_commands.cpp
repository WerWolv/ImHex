#include<iostream>
#include<stdlib.h>

#include <hex/api/plugin_manager.hpp>
#include <hex/helpers/logger.hpp>

namespace hex::init {
    void processArguments(std::vector<std::string> args){
        if (args.empty()) return;

        std::optional<SubCommand> selectedSubCommand;

        // default subcommand if no subcommand has been specified
        std::optional<SubCommand> openSubCommand;

        for (auto &plugin : PluginManager::getPlugins()) {
            for (auto &subCommand : plugin.getSubCommands()) {
                if (hex::format("--{}", subCommand.commandKey) == args[0]) {
                    if (selectedSubCommand) {
                        log::error("Multiple subcommands entered: {} and {}", selectedSubCommand->commandKey, args[0]);
                        exit(EXIT_FAILURE);
                    } else {
                        selectedSubCommand = subCommand;
                    }
                } else if (subCommand.commandKey == "open") {
                    openSubCommand = subCommand;
                }
            }
        }


        if (selectedSubCommand) {
            args.erase(args.begin()); // remove subcommand argument
            selectedSubCommand->callback(args);
        } else {
            openSubCommand->callback(args);
        }
        
    }
}
