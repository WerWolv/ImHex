#pragma once

#include <vector>
#include <string>
#include <functional>

namespace hex::subcommands {

    /**
     * @brief Internal method - takes all the arguments ImHex received from the command line,
     * and determine which subcommands to run, with which arguments.
     * In some cases, the subcommand or this function directly might exit the program
     * (e.g. --help, or when forwarding providers to open to another instance)
     * and so this function might not return
     */
    void processArguments(const std::vector<std::string> &args);

    
    /**
     * @brief Forward the given command to the main instance (might be this instance)
     * The callback will be executed after EventImHexStartupFinished
     */
    void forwardSubCommand(const std::string &cmdName, const std::vector<std::string> &args);

    using ForwardCommandHandler = std::function<void(const std::vector<std::string> &)>;
    
    /**
     * @brief Register the handler for this specific command name
     */
    void registerSubCommand(const std::string &cmdName, const ForwardCommandHandler &handler);

}
