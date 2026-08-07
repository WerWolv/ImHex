#pragma once

#include <string>
#include <span>

namespace hex::plugin::builtin {

    int handleVersionCommand(std::span<const std::string> args);
    int handleVersionShortCommand(std::span<const std::string> args);
    int handleHelpCommand(std::span<const std::string> args);
    int handlePluginsCommand(std::span<const std::string> args);
    int handleLanguageCommand(std::span<const std::string> args);
    int handleVerboseCommand(std::span<const std::string> args);

    int handleOpenCommand(std::span<const std::string> args);
    int handleNewCommand(std::span<const std::string> args);

    int handleSelectCommand(std::span<const std::string> args);
    int handlePatternCommand(std::span<const std::string> args);
    int handleCalcCommand(std::span<const std::string> args);
    int handleHashCommand(std::span<const std::string> args);
    int handleEncodeCommand(std::span<const std::string> args);
    int handleDecodeCommand(std::span<const std::string> args);
    int handleMagicCommand(std::span<const std::string> args);
    int handlePatternLanguageCommand(std::span<const std::string> args);
    int handleHexdumpCommand(std::span<const std::string> args);
    int handleDemangleCommand(std::span<const std::string> args);
    int handleSettingsResetCommand(std::span<const std::string> args);
    int handleDebugModeCommand(std::span<const std::string> args);
    int handleValidatePluginCommand(std::span<const std::string> args);
    int handleSaveEditorCommand(std::span<const std::string> args);
    int handleFileInfoCommand(std::span<const std::string> args);
    int handleMCPCommand(std::span<const std::string> args);
    int handleScalingCommand(std::span<const std::string> args);

    void registerCommandForwarders();

}