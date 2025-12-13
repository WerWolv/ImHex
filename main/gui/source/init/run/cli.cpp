#include <hex/api/event_manager.hpp>

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
                log::error("Failed to get command line arguments");
                std::exit(EXIT_FAILURE);
            }

            // Skip the first argument (the executable path) and convert the rest to a vector of UTF-8 strings
            for (int i = 1; i < argc; i += 1) {
                std::wstring wcharArg = convertedCommandLine[i];
                auto utf8Arg = wolv::util::wstringToUtf8(wcharArg);
                if (!utf8Arg.has_value()) {
                    log::error("Failed to convert command line arguments to UTF-8");
                    std::exit(EXIT_FAILURE);
                }

                args.push_back(*utf8Arg);
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

        // Process our own global flags first
        {
            std::vector<std::string> remaining;
            remaining.reserve(args.size());

            for (size_t i = 0; i < args.size(); i += 1) {
                const auto &arg = args[i];
                if (arg == "--readonly" || arg == "-r") {
                    ImHexApi::System::impl::setReadOnlyMode(true);
                    continue;
                }
                remaining.push_back(arg);
            }

            args.swap(remaining);
        }

        // Process the arguments (subcommands) after handling globals
        hex::subcommands::processArguments(args);

        // Explicitly don't unload plugins again here.
        // Certain CLI commands configure things inside of plugins and then let ImHex start up normally
        // If plugins were to be unloaded here, this setup would be reset.
        // PluginManager::load() will be executed again later on, but it will not load any more plugins into the
        // address space. But they will be properly initialized at that point.
    }

}