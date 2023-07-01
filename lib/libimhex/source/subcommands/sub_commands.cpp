#include<iostream>
#include<stdlib.h>

#include <hex/api/plugin_manager.hpp>
#include <hex/helpers/logger.hpp>

namespace hex::init {

    std::optional<SubCommand> findSubCommand(const std::string &arg) {
        for (auto &plugin : PluginManager::getPlugins()) {
            for (auto &subCommand : plugin.getSubCommands()) {
                if (hex::format("--{}", subCommand.commandKey) == arg) {
                    return subCommand;
                }
            }
        }
        return { };
    }

    void processArguments(const std::vector<std::string> &args){
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
        while(argsIter != args.end()) {
            const std::string &arg = *argsIter;

            if (arg == "--othercmd") {
                // save command to run
                if (currentSubCommand) {
                    subCommands.push_back({*currentSubCommand, currentSubCommandArgs});
                }

                currentSubCommand = { };
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
    }
}
