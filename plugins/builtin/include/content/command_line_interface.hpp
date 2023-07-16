#pragma once

#include <string>
#include <vector>

namespace hex::plugin::builtin {

    void handleVersionCommand(const std::vector<std::string> &args);
    void handleHelpCommand(const std::vector<std::string> &args);
    void handlePluginsCommand(const std::vector<std::string> &args);

    void handleOpenCommand(const std::vector<std::string> &args);

    void handleCalcCommand(const std::vector<std::string> &args);
    void handleHashCommand(const std::vector<std::string> &args);
    void handleEncodeCommand(const std::vector<std::string> &args);
    void handleDecodeCommand(const std::vector<std::string> &args);


    void registerCommandForwarders();

}