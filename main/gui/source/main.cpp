#include <hex.hpp>

#include <hex/helpers/logger.hpp>

#include "window.hpp"
#include "crash_handlers.hpp"
#include "messaging.hpp"

#include "init/splash_window.hpp"

#include <hex/api/task_manager.hpp>
#include <hex/api/plugin_manager.hpp>

#include <hex/helpers/fs.hpp>
#include "hex/subcommands/subcommands.hpp"

#include <wolv/io/fs.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <shellapi.h>
    #include <codecvt>
#endif

using namespace hex;

namespace hex::init { int runImHex(); }

namespace {

    /**
     * @brief Handles commands passed to ImHex via the command line
     * @param argc Argument count
     * @param argv Argument values
     */
    void handleCommandLineInterface(int argc, char **argv) {
        std::vector<std::string> args;

        #if defined (OS_WINDOWS)
            hex::unused(argv);

            // On Windows, argv contains UTF-16 encoded strings, so we need to convert them to UTF-8
            auto convertedCommandLine = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
            if (convertedCommandLine == nullptr) {
                log::error("Failed to convert command line arguments to UTF-8");
                std::exit(EXIT_FAILURE);
            }

            for (int i = 1; i < argc; i += 1) {
                std::wstring wcharArg = convertedCommandLine[i];
                std::string  utf8Arg  = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(wcharArg);

                args.push_back(utf8Arg);
            }

            ::LocalFree(convertedCommandLine);
        #else
            // Skip the first argument (the executable path) and convert the rest to a vector of strings
            args = { argv + 1, argv + argc };
        #endif


        // Load all plugins but don't initialize them
        for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Plugins)) {
            PluginManager::load(dir);
        }

        // Setup messaging system to allow sending commands to the main ImHex instance
        hex::messaging::setupMessaging();

        // Process the arguments
        hex::subcommands::processArguments(args);

        // Unload plugins again
        PluginManager::unload();
    }

    /**
     * @brief Checks if the portable version of ImHex is installed
     * @note The portable version is detected by the presence of a file named "PORTABLE" in the same directory as the executable
     * @return True if ImHex is running in portable mode, false otherwise
     */
    bool isPortableVersion() {
        if (const auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value()) {
            const auto flagFile = executablePath->parent_path() / "PORTABLE";

            if (wolv::io::fs::exists(flagFile) && wolv::io::fs::isRegularFile(flagFile))
                return true;
        }

        return false;
    }

}

/**
 * @brief Main entry point of ImHex
 * @param argc Argument count
 * @param argv Argument values
 * @return Exit code
 */
int main(int argc, char **argv) {
    TaskManager::setCurrentThreadName("Main");
    Window::initNative();
    crash::setupCrashHandlers();

    if (argc > 1) {
        handleCommandLineInterface(argc, argv);
    }

    log::info("Welcome to ImHex {}!", ImHexApi::System::getImHexVersion());
    log::info("Compiled using commit {}@{}", ImHexApi::System::getCommitBranch(), ImHexApi::System::getCommitHash());
    log::info("Running on {} {} ({})", ImHexApi::System::getOSName(), ImHexApi::System::getOSVersion(), ImHexApi::System::getArchitecture());

    ImHexApi::System::impl::setPortableVersion(isPortableVersion());

    return init::runImHex();
}
