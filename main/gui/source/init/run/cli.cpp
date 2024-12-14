#include "messaging.hpp"

#include <hex/subcommands/subcommands.hpp>

#include <hex/api/plugin_manager.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/default_paths.hpp>

#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>


#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <shellapi.h>
#endif

namespace hex::init {

    /**
     * @brief Handles commands passed to ImHex via the command line
     * @param argc Argument count
     * @param argv Argument values
     */
    void runCommandLine(int argc, char **argv) {
        // Suspend logging while processing command line arguments so
        // we don't spam the console with log messages while printing
        // CLI tool messages
        log::suspendLogging();
        ON_SCOPE_EXIT {
            log::resumeLogging();
        };

        std::vector<std::string> args;

        #if defined (OS_WINDOWS)
            std::ignore = argv;

            // On Windows, argv contains UTF-16 encoded strings, so we need to convert them to UTF-8
            auto convertedCommandLine = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
            if (convertedCommandLine == nullptr) {
                log::error("Failed to convert command line arguments to UTF-8");
                std::exit(EXIT_FAILURE);
            }

            // Skip the first argument (the executable path) and convert the rest to a vector of UTF-8 strings
            for (int i = 1; i < argc; i += 1) {
                std::wstring wcharArg = convertedCommandLine[i];
                std::string utf8Arg = wolv::util::wstringToUtf8(wcharArg);

                args.push_back(utf8Arg);
            }

            ::LocalFree(convertedCommandLine);
        #else
            // Skip the first argument (the executable path) and convert the rest to a vector of strings
            args = { argv + 1, argv + argc };
        #endif


        // Load all plugins but don't initialize them
        PluginManager::loadLibraries();
        for (const auto &dir : paths::Plugins.read()) {
            PluginManager::load(dir);
        }

        // Setup messaging system to allow sending commands to the main ImHex instance
        hex::messaging::setupMessaging();

        // Process the arguments
        hex::subcommands::processArguments(args);

        // Unload plugins again
        PluginManager::unload();
    }

}