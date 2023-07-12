#include <hex.hpp>

#include <hex/helpers/logger.hpp>

#include "window.hpp"
#include "crash_handlers.hpp"
#include "messaging.hpp"

#include "init/splash_window.hpp"
#include "init/tasks.hpp"

#include <hex/api/task.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/plugin_manager.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/helpers/fs.hpp>
#include "hex/subcommands/subcommands.hpp"

#include <wolv/io/fs.hpp>
#include <wolv/utils/guards.hpp>

using namespace hex;

void initPlugins() {
    for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Plugins)) {
        PluginManager::load(dir);
    }
}

int main(int argc, char **argv, char **envp) {
    hex::crash::setupCrashHandlers();
    hex::unused(envp);

    std::vector<std::string> args(argv + 1, argv + argc);

    initPlugins();

    // Check if ImHex is installed in portable mode
    {
        if (const auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value()) {
            const auto flagFile = executablePath->parent_path() / "PORTABLE";

            if (wolv::io::fs::exists(flagFile) && wolv::io::fs::isRegularFile(flagFile))
                ImHexApi::System::impl::setPortableVersion(true);
        }
    }

    bool shouldRestart = false;
    do {
        // Register an event to handle restarting of ImHex
        EventManager::subscribe<RequestRestartImHex>([&]{ shouldRestart = true; });
        shouldRestart = false;

        // Initialization
        {
            Window::initNative();

            log::info("Welcome to ImHex {}!", ImHexApi::System::getImHexVersion());
            log::info("Compiled using commit {}@{}", ImHexApi::System::getCommitBranch(), ImHexApi::System::getCommitHash());

            hex::messaging::setupMessaging();
            hex::subcommands::processArguments(args);

            init::WindowSplash splashWindow;

            // Add initialization tasks to run
            TaskManager::init();
            for (const auto &[name, task, async] : init::getInitTasks())
                splashWindow.addStartupTask(name, task, async);

            // Draw the splash window while tasks are running
            if (!splashWindow.loop())
                ImHexApi::System::getInitArguments().insert({ "tasks-failed", {} });
        }

        log::info("Running on {} {} ({})", ImHexApi::System::getOSName(), ImHexApi::System::getOSVersion(), ImHexApi::System::getArchitecture());
        log::info("Using '{}' GPU", ImHexApi::System::getGPUVendor());

        // Clean up everything after the main window is closed
        auto exitHandler = [](auto){
            for (const auto &[name, task, async] : init::getExitTasks())
                task();
            TaskManager::exit();
        };

        ON_SCOPE_EXIT { exitHandler(0); };

        // Main window
        {
            Window window;

            // Open file that has been requested to be opened through other, OS-specific means
            if (auto path = hex::getInitialFilePath(); path.has_value()) {
                EventManager::post<RequestOpenFile>(path.value());
            }

            // Render the main window

            EventManager::post<EventImHexStartupFinished>();
            window.loop();
        }

    } while (shouldRestart);

    return EXIT_SUCCESS;
}
