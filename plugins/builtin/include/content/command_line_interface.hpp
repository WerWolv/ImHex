#pragma once

#include <hex/plugin.hpp>

#include <string>
#include <span>

namespace hex::plugin::builtin {

    CommandResult handleVersionCommand(std::span<const std::string> args);
    CommandResult handleVersionShortCommand(std::span<const std::string> args);
    CommandResult handleHelpCommand(std::span<const std::string> args);
    CommandResult handlePluginsCommand(std::span<const std::string> args);
    CommandResult handleLanguageCommand(std::span<const std::string> args);
    CommandResult handleVerboseCommand(std::span<const std::string> args);

    CommandResult handleOpenCommand(std::span<const std::string> args);
    CommandResult handleNewCommand(std::span<const std::string> args);

    CommandResult handleSelectCommand(std::span<const std::string> args);
    CommandResult handlePatternCommand(std::span<const std::string> args);
    CommandResult handleCalcCommand(std::span<const std::string> args);
    CommandResult handleHashCommand(std::span<const std::string> args);
    CommandResult handleEncodeCommand(std::span<const std::string> args);
    CommandResult handleDecodeCommand(std::span<const std::string> args);
    CommandResult handleMagicCommand(std::span<const std::string> args);
    CommandResult handlePatternLanguageCommand(std::span<const std::string> args);
    CommandResult handleHexdumpCommand(std::span<const std::string> args);
    CommandResult handleDemangleCommand(std::span<const std::string> args);
    CommandResult handleSettingsResetCommand(std::span<const std::string> args);
    CommandResult handleDebugModeCommand(std::span<const std::string> args);
    CommandResult handleValidatePluginCommand(std::span<const std::string> args);
    CommandResult handleSaveEditorCommand(std::span<const std::string> args);
    CommandResult handleFileInfoCommand(std::span<const std::string> args);
    CommandResult handleMCPCommand(std::span<const std::string> args);
    CommandResult handleScalingCommand(std::span<const std::string> args);

    void registerCommandForwarders();

}