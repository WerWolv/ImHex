#pragma once

#include <string>
#include <vector>

namespace hex::plugin::builtin {

    void handleVersionCommand(const std::vector<std::string> &args);
    void handleVersionShortCommand(const std::vector<std::string> &args);
    void handleHelpCommand(const std::vector<std::string> &args);
    void handlePluginsCommand(const std::vector<std::string> &args);
    void handleLanguageCommand(const std::vector<std::string> &args);
    void handleVerboseCommand(const std::vector<std::string> &args);

    void handleOpenCommand(const std::vector<std::string> &args);
    void handleNewCommand(const std::vector<std::string> &args);

    void handleSelectCommand(const std::vector<std::string> &args);
    void handlePatternCommand(const std::vector<std::string> &args);
    void handleCalcCommand(const std::vector<std::string> &args);
    void handleHashCommand(const std::vector<std::string> &args);
    void handleEncodeCommand(const std::vector<std::string> &args);
    void handleDecodeCommand(const std::vector<std::string> &args);
    void handleMagicCommand(const std::vector<std::string> &args);
    void handlePatternLanguageCommand(const std::vector<std::string> &args);
    void handleHexdumpCommand(const std::vector<std::string> &args);
    void handleDemangleCommand(const std::vector<std::string> &args);
    void handleSettingsResetCommand(const std::vector<std::string> &args);
    void handleDebugModeCommand(const std::vector<std::string> &args);
    void handleValidatePluginCommand(const std::vector<std::string> &args);
    void handleSaveEditorCommand(const std::vector<std::string> &args);
    void handleFileInfoCommand(const std::vector<std::string> &args);
    void handleMCPCommand(const std::vector<std::string> &args);

    void registerCommandForwarders();

}